#include <pwfcomb.h>
#include <pwfcombqueue.h>
#include <numa.h>

static const int LOCAL_POOL_SIZE = _SIM_PERSISTENT_LOCAL_POOL_SIZE_;

static const uint64_t NVMEM_CACHE_LINE_SIZE = 64;
static const uint64_t NEG_NVMEM_CACHE_LINE_SIZE = ~(64 - 1);

static __thread int fad_division_enqueue = -1;
static __thread int fad_division_dequeue = -1;

static __thread Node **clNewItems;
static __thread uint64_t clNewItems_size;

static inline void EnqStateCopy(PWFCombQueueEnqRec *dest, PWFCombQueueEnqRec *src);
static inline void DeqStateCopy(PWFCombQueueDeqState *dest, PWFCombQueueDeqState *src);

static inline void EnqLinkQueue(PWFCombQueueStruct *queue, PWFCombQueueEnqRec *pst);
static inline void DeqLinkQueue(PWFCombQueueStruct *queue, PWFCombQueueDeqState *pst);

inline static void EnqLinkQueue(PWFCombQueueStruct *queue, PWFCombQueueEnqRec *pst) {
    if (pst->first != NULL) {
        synchCASPTR(&pst->first->next, NULL, pst->last);
        synchFlushPersistentMemory((void *)&pst->first->next, sizeof(Node *));
    }
}

inline static void DeqLinkQueue(PWFCombQueueStruct *queue, PWFCombQueueDeqState *pst) {
    pointer_t ES;
    PWFCombQueueEnqRec *enq_pst;

    ES.raw_data = queue->Epstate->S.raw_data;
    enq_pst = queue->EState[ES.struct_data.index];

    if (pst->head->next == NULL) {
        volatile Node *last = enq_pst->last;
        volatile Node *first = enq_pst->first;
        synchFullFence();
        if (first != NULL && last != NULL && ES.raw_data == queue->Epstate->S.raw_data) {
            synchCASPTR(&first->next, NULL, last);
            synchFlushPersistentMemory((void *)&first->next, sizeof(Node *));
        }
    }
}

static inline void EnqStateCopy(PWFCombQueueEnqRec *dest, PWFCombQueueEnqRec *src) {
    // copy everything except 'deactivate' and 'index' fields
    memcpy(&dest->first, &src->first, PWFCombQueueEnqStateSize(src->deactivate.nthreads) - 2 * sizeof(ToggleVector));
}

static inline void DeqStateCopy(PWFCombQueueDeqState *dest, PWFCombQueueDeqState *src) {
    // copy everything except 'deactivate', 'index' and 'return_val' fields
    memcpy(&dest->head, &src->head, PWFCombQueueDeqStateSize(src->deactivate.nthreads) - 2 * sizeof(ToggleVector) - sizeof(RetVal *));
}

void PWFCombQueueThreadStateInit(PWFCombQueueStruct *queue, PWFCombQueueThreadState *th_state, int pid) {
    int i;
    
    TVEC_INIT(&th_state->mask, queue->nthreads);
    TVEC_INIT(&th_state->deq_index, queue->nthreads);
    TVEC_INIT(&th_state->enq_index, queue->nthreads);
    TVEC_INIT(&th_state->diffs, queue->nthreads);
    TVEC_INIT(&th_state->l_activate, queue->nthreads);
    TVEC_INIT(&th_state->diffs_copy, queue->nthreads);

    TVEC_SET_BIT(&th_state->mask, pid);
    TVEC_NEGATIVE(&th_state->enq_index, &th_state->mask);
    synchInitPoolPersistent(&th_state->pool_node, sizeof(Node));

    TVEC_SET_ZERO(&th_state->mask);
    TVEC_SET_BIT(&th_state->mask, pid);
    TVEC_NEGATIVE(&th_state->deq_index, &th_state->mask);
    th_state->backoff = 1;

    clNewItems = synchGetAlignedMemory(CACHE_LINE_SIZE, queue->nthreads * sizeof(Node **));
    for (i = 0; i < queue->nthreads; i++) {
        clNewItems[i] = NULL;
    }
    clNewItems_size = 0;
}

void PWFCombQueueInit(PWFCombQueueStruct *queue, uint32_t nthreads, int max_backoff) {
    pointer_t tmp_sp;
    int i;

    queue->nthreads = nthreads;
    queue->ERequest = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(PWFCombRequestRec));
    queue->Ecomb_round = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(uint64_t *));
    for (i = 0; i < nthreads; i++) {
        queue->ERequest[i].arg = 0;
        queue->ERequest[i].valid = false;
        queue->Ecomb_round[i] = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(uint64_t));
        int j;
        for (j = 0; j < nthreads; j++) {
            queue->Ecomb_round[i][j] = 0;
        }
    }
    queue->DRequest = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(PWFCombRequestRec));
    queue->Dcomb_round = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(uint64_t *));
    for (i = 0; i < nthreads; i++) {
        queue->DRequest[i].arg = 0;
        queue->DRequest[i].valid = false;
        queue->Dcomb_round[i] = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(uint64_t));
        int j;
        for (j = 0; j < nthreads; j++) {
            queue->Dcomb_round[i][j] = 0;
        }
    }
    for (i = 0; i < _SIM_PERSISTENT_FAD_DIVISIONS_; i++) {
        TVEC_INIT_AT((ToggleVector *)&queue->activate_enq[i], nthreads, synchGetPersistentMemory(CACHE_LINE_SIZE, _TVEC_VECTOR_SIZE(nthreads)));
        TVEC_SET_ZERO((ToggleVector *)&queue->activate_enq[i]);
        TVEC_INIT_AT((ToggleVector *)&queue->activate_deq[i], nthreads, synchGetPersistentMemory(CACHE_LINE_SIZE, _TVEC_VECTOR_SIZE(nthreads)));
        TVEC_SET_ZERO((ToggleVector *)&queue->activate_deq[i]);                     
    }

    queue->Epstate = synchGetPersistentMemory(S_CACHE_LINE_SIZE, sizeof(PWFCombQueuePersistentState));
    queue->Dpstate = synchGetPersistentMemory(2*S_CACHE_LINE_SIZE, sizeof(PWFCombQueuePersistentState));
    tmp_sp.struct_data.index = LOCAL_POOL_SIZE * nthreads;
    tmp_sp.struct_data.seq = 0L;
    queue->Epstate->S = tmp_sp;
    queue->Dpstate->S = tmp_sp;

    queue->EState = synchGetPersistentMemory(CACHE_LINE_SIZE, (LOCAL_POOL_SIZE * nthreads + 1) * sizeof(PWFCombQueueEnqRec *));
    queue->DState = synchGetPersistentMemory(CACHE_LINE_SIZE, (LOCAL_POOL_SIZE * nthreads + 1) * sizeof(PWFCombQueueDeqState *));
    
    for (i = 0; i < LOCAL_POOL_SIZE * nthreads + 1; i++) {
        queue->EState[i] = synchGetPersistentMemory(CACHE_LINE_SIZE, PWFCombQueueEnqStateSize(nthreads));
        queue->DState[i] = synchGetPersistentMemory(CACHE_LINE_SIZE, PWFCombQueueDeqStateSize(nthreads));

        TVEC_INIT_AT(&queue->EState[i]->deactivate, nthreads, ((void *)queue->EState[i]->__flex));
        TVEC_INIT_AT(&queue->EState[i]->index, nthreads, ((void *)queue->EState[i]->__flex) + _TVEC_VECTOR_SIZE(nthreads));

        TVEC_INIT_AT(&queue->DState[i]->deactivate, nthreads, ((void *)queue->DState[i]->__flex));
        TVEC_INIT_AT(&queue->DState[i]->index, nthreads, ((void *)queue->DState[i]->__flex) + _TVEC_VECTOR_SIZE(nthreads));
        queue->DState[i]->return_val = ((void *)queue->DState[i]->__flex) + 2 * _TVEC_VECTOR_SIZE(nthreads);
    }

    queue->Eflush = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(uint64_t *) * (nthreads + 1));
    queue->Dflush = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(uint64_t *) * (nthreads + 1));
    for (i = 0; i < nthreads + 1; i++) {
        queue->Eflush[i] = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(uint64_t));
        queue->Dflush[i] = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(uint64_t));
    }
    // Initializing queue's state
    // --------------------------
    queue->guard.val = GUARD_VALUE;
    queue->guard.next = NULL;
    *queue->Eflush[nthreads] = 0;
    TVEC_SET_ZERO((ToggleVector *) &queue->EState[LOCAL_POOL_SIZE * nthreads]->deactivate);
    TVEC_SET_ZERO((ToggleVector *) &queue->EState[LOCAL_POOL_SIZE * nthreads]->index);
    queue->EState[LOCAL_POOL_SIZE * nthreads]->tail = &queue->guard;
    queue->EState[LOCAL_POOL_SIZE * nthreads]->first = NULL;
    queue->EState[LOCAL_POOL_SIZE * nthreads]->last = NULL;
    *queue->Dflush[nthreads] = 0;
    TVEC_SET_ZERO((ToggleVector *) &queue->DState[LOCAL_POOL_SIZE * nthreads]->deactivate);
    TVEC_SET_ZERO((ToggleVector *) &queue->DState[LOCAL_POOL_SIZE * nthreads]->index);
    queue->DState[LOCAL_POOL_SIZE * nthreads]->head = &queue->guard;
#ifdef DEBUG
    queue->EState[LOCAL_POOL_SIZE * nthreads]->counter = 0L;
    queue->DState[LOCAL_POOL_SIZE * nthreads]->counter = 0L;
#endif
    queue->MAX_BACK = max_backoff * 100;

    synchFullFence();
}

void PWFCombQueueEnqueue(PWFCombQueueStruct *queue, PWFCombQueueThreadState *th_state, ArgVal arg, int pid) {
    ToggleVector *diffs = &th_state->diffs,
                 *l_activate = &th_state->l_activate;
    pointer_t old_sp, new_sp;
    int i, j, enq_counter, prefix;
    PWFCombQueueEnqRec *lsp_data, *sp_data;
    Node *node, *llist;
    int curr_pool_index;
    uint64_t l_val;

    if (fad_division_enqueue == -1) {
        fad_division_enqueue = numa_node_of_cpu(synchGetPreferedCore()) % _SIM_PERSISTENT_FAD_DIVISIONS_;
        if (fad_division_enqueue == -1) fad_division_enqueue = 0;
    }

    queue->ERequest[pid].arg = arg;
    queue->ERequest[pid].valid = true;
    synchFullFence();

    int mybank = TVEC_GET_BANK_OF_BIT(pid, queue->nthreads);
    TVEC_NEGATIVE_BANK(&th_state->enq_index, &th_state->enq_index, mybank);
    TVEC_ATOMIC_ADD_BANK(&queue->activate_enq[fad_division_enqueue], &th_state->enq_index, mybank);            // toggle pid's bit in activate_enq, Fetch&Add acts as a full write-barrier

    if (!synchIsSystemOversubscribed()) { 
        volatile int k;
        int backoff_limit;

        if (synchFastRandomRange(1, queue->nthreads) > 1) {
            backoff_limit = synchFastRandomRange(th_state->backoff >> 1, th_state->backoff);
            for (k = 0; k < backoff_limit; k++)
                ;
        }
    } else if (synchFastRandomRange(1, queue->nthreads) > 4) {
        synchResched();    
    }

    for (j = 0; j < 2; j++) {
        old_sp = queue->Epstate->S;
        sp_data = queue->EState[old_sp.struct_data.index];
        TVEC_XOR_BANKS(diffs, &queue->activate_enq[fad_division_enqueue], &sp_data->deactivate, mybank);                               // determine the set of active processes
        l_val = *queue->Eflush[old_sp.struct_data.index/LOCAL_POOL_SIZE]; 
        if (old_sp.raw_data != queue->Epstate->S.raw_data)
            continue;
        if (!TVEC_IS_SET(diffs, pid))                                                           // if the operation has already been deactivate return
            break;

        uint64_t local_index = pid * LOCAL_POOL_SIZE + TVEC_IS_SET(&sp_data->index, pid);
        lsp_data = queue->EState[local_index];
        EnqStateCopy(lsp_data, sp_data);

        TVEC_SET_ZERO(l_activate);
        for (i = 0; i < _SIM_PERSISTENT_FAD_DIVISIONS_; i++) {
            TVEC_OR(l_activate, l_activate, (ToggleVector *)&queue->activate_enq[i]);                // This is an atomic read, since activate_enq is volatile
        }

        if (old_sp.raw_data != queue->Epstate->S.raw_data)
            continue;

        for (i = 0; i < queue->nthreads; i++) {
            clNewItems[i] = NULL;
        }
        TVEC_XOR(diffs, &lsp_data->deactivate, l_activate);
        TVEC_COPY(&th_state->diffs_copy, diffs);
        EnqLinkQueue(queue, lsp_data);
        enq_counter = 1;
        node = synchAllocObj(&th_state->pool_node);                                                    
        node->next = NULL;
        node->val = arg;
        llist = node;
        TVEC_REVERSE_BIT(diffs, pid);
        clNewItems[0] = (void *)(node);
        clNewItems_size = 1;
#ifdef DEBUG
        lsp_data->counter += 1;
#endif
        for (i = 0, prefix = 0; i < diffs->tvec_cells; i++, prefix += _TVEC_BIWORD_SIZE_) {
            while (diffs->cell[i] != 0L) {
                register int pos, proc_id;

                pos = synchBitSearchFirst(diffs->cell[i]);
                proc_id = prefix + pos;
                diffs->cell[i] ^= ((bitword_t)1) << pos;
                if (queue->ERequest[proc_id].valid == false) {
                    TVEC_REVERSE_BIT(l_activate, proc_id);
                    continue;
                }
                enq_counter++;
#ifdef DEBUG
                lsp_data->counter += 1;
#endif
                node->next = synchAllocObj(&th_state->pool_node);
                node = (Node *)node->next;
                node->next = NULL;
                node->val = queue->ERequest[proc_id].arg;

                uint64_t new_item_ptr;
                bool found = false;
                int k;

                new_item_ptr = (uint64_t)node & NEG_NVMEM_CACHE_LINE_SIZE;
                for (k = 0; k < clNewItems_size; k++) {
                    if (new_item_ptr == (uint64_t)clNewItems[k]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    clNewItems[clNewItems_size] = (void *)(new_item_ptr);
                    clNewItems_size++;
                }

            }
        }

        lsp_data->first = lsp_data->tail;
        lsp_data->last = llist;
        lsp_data->tail = node;
        TVEC_COPY(&lsp_data->deactivate, l_activate);
        TVEC_REVERSE_BIT(&lsp_data->index, pid);

        new_sp.struct_data.seq = old_sp.struct_data.seq + 1;
        new_sp.struct_data.index = local_index;

        for (i = 0; i < clNewItems_size; i++) {
            if (old_sp.raw_data != queue->Epstate->S.raw_data)
                break;
            synchFlushPersistentMemory((void *)clNewItems[i], NVMEM_CACHE_LINE_SIZE);
        }
        
        if (old_sp.raw_data == queue->Epstate->S.raw_data) {
            synchFlushPersistentMemory(lsp_data, PWFCombQueueEnqStateSize(queue->nthreads));
            synchDrainPersistentMemory();

            if (!l_val%2) {
                l_val++;
            } 
            else {
                l_val+=2;
            }
            *queue->Eflush[new_sp.struct_data.index/LOCAL_POOL_SIZE] = l_val;

            for (i = 0, prefix = 0; i < th_state->diffs_copy.tvec_cells; i++, prefix += _TVEC_BIWORD_SIZE_) {
                while (th_state->diffs_copy.cell[i] != 0L) {
                    register int pos, proc_id;
                    pos = synchBitSearchFirst(th_state->diffs_copy.cell[i]);
                    proc_id = prefix + pos;
                    th_state->diffs_copy.cell[i] ^= ((bitword_t)1) << pos;
                    queue->Ecomb_round[pid][proc_id] = l_val;
                }
            }

            if (old_sp.raw_data == queue->Epstate->S.raw_data && synchCAS64(&queue->Epstate->S, old_sp.raw_data, new_sp.raw_data)) {
                EnqLinkQueue(queue, lsp_data);
                synchFlushPersistentMemory((void *)&queue->Epstate->S, sizeof(uint64_t));
                synchDrainPersistentMemory();
                synchCAS64(queue->Eflush[new_sp.struct_data.index/LOCAL_POOL_SIZE], l_val, l_val+1);
                th_state->backoff = (th_state->backoff >> 1) | 1;
                return;
            }
        } else {
            if (th_state->backoff < queue->MAX_BACK) th_state->backoff <<= 1;
            synchRollback(&th_state->pool_node, enq_counter);
        }
    }

    curr_pool_index = queue->Epstate->S.struct_data.index;
    l_val = *queue->Eflush[curr_pool_index/LOCAL_POOL_SIZE];
    if (l_val%2 == 1 && l_val == queue->Ecomb_round[curr_pool_index/LOCAL_POOL_SIZE][pid]) {
        synchFlushPersistentMemory((void *)&queue->Epstate->S, sizeof(uint64_t));
        synchDrainPersistentMemory();
        synchCAS64(queue->Eflush[curr_pool_index/LOCAL_POOL_SIZE], l_val, l_val+1);
    }

    return;
}

RetVal PWFCombQueueDequeue(PWFCombQueueStruct *queue, PWFCombQueueThreadState *th_state, int pid) {
    ToggleVector *diffs = &th_state->diffs,
                 *l_activate = &th_state->l_activate;
    PWFCombQueueDeqState *lsp_data, *sp_data;
    int i, j, prefix;
    pointer_t old_sp, new_sp;
    volatile Node *node;
    int curr_pool_index;
    uint64_t l_val;

    if (fad_division_dequeue == -1) {
        fad_division_dequeue = numa_node_of_cpu(synchGetPreferedCore()) % _SIM_PERSISTENT_FAD_DIVISIONS_;
        if (fad_division_dequeue == -1) fad_division_dequeue = 0;
    }

    if (!queue->DRequest[pid].valid) {
        queue->DRequest[pid].valid = 1;
    }
    synchFullFence();                             

    int mybank = TVEC_GET_BANK_OF_BIT(pid, queue->nthreads);
    TVEC_NEGATIVE_BANK(&th_state->deq_index, &th_state->deq_index, mybank);
    TVEC_ATOMIC_ADD_BANK(&queue->activate_deq[fad_division_dequeue], &th_state->deq_index, mybank); // toggle pid's bit in activate_deq, Fetch&Add acts as a full write-barrier

    if (!synchIsSystemOversubscribed()) { 
        volatile int k;
        int backoff_limit;

        if (synchFastRandomRange(1, queue->nthreads) > 1) {
            backoff_limit =  synchFastRandomRange(th_state->backoff >> 1, th_state->backoff);
            for (k = 0; k < backoff_limit; k++)
                ;
        }
    } else if (synchFastRandomRange(1, queue->nthreads) > 4) {
        synchResched();
    }

    for (j = 0; j < 2; j++) {
        old_sp = queue->Dpstate->S;
        sp_data = queue->DState[old_sp.struct_data.index];
        TVEC_XOR_BANKS(diffs, &queue->activate_deq[fad_division_dequeue], &sp_data->deactivate, mybank);                               // determine the set of active processes
        l_val = *queue->Dflush[old_sp.struct_data.index/LOCAL_POOL_SIZE]; 
        if (old_sp.raw_data != queue->Dpstate->S.raw_data)
            continue;
        if (!TVEC_IS_SET(diffs, pid))                                                           // if the operation has already been deactivate return
            break;

        uint64_t local_index = pid * LOCAL_POOL_SIZE + TVEC_IS_SET(&sp_data->index, pid);
        lsp_data = queue->DState[local_index];
        DeqStateCopy(lsp_data, sp_data);

        TVEC_SET_ZERO(l_activate);
        for (i = 0; i < _SIM_PERSISTENT_FAD_DIVISIONS_; i++) {
            TVEC_OR(l_activate, l_activate, (ToggleVector *)&queue->activate_deq[i]);            // This is an atomic read, since activate_deq is volatile
        }

        if (old_sp.raw_data != queue->Dpstate->S.raw_data)
            continue;

        TVEC_XOR(diffs, &lsp_data->deactivate, l_activate);
        TVEC_COPY(&th_state->diffs_copy, diffs);
        DeqLinkQueue(queue, lsp_data);
        for (i = 0, prefix = 0; i < diffs->tvec_cells; i++, prefix += _TVEC_BIWORD_SIZE_) {
            while (diffs->cell[i] != 0L) {
                register int pos, proc_id;

                pos = synchBitSearchFirst(diffs->cell[i]);
                proc_id = prefix + pos;
                diffs->cell[i] ^= ((bitword_t)1) << pos;
                if (queue->DRequest[proc_id].valid == false) {
                    TVEC_REVERSE_BIT(l_activate, proc_id);
                    continue;
                }
#ifdef DEBUG
                lsp_data->counter += 1;
#endif
                node = lsp_data->head->next;
                if (node == NULL) DeqLinkQueue(queue, lsp_data);
                node = lsp_data->head->next;
                if (node != NULL) {
                    lsp_data->return_val[proc_id] = node->val;
                    lsp_data->head = (Node *)node;
                } else lsp_data->return_val[proc_id] = GUARD_VALUE;
            }
        }
        TVEC_COPY(&lsp_data->deactivate, l_activate);
        TVEC_REVERSE_BIT(&lsp_data->index, pid);

        new_sp.struct_data.seq = old_sp.struct_data.seq + 1;
        new_sp.struct_data.index = local_index;

        if (old_sp.raw_data == queue->Dpstate->S.raw_data) {
            synchFlushPersistentMemory(lsp_data, PWFCombQueueDeqStateSize(queue->nthreads));
            synchDrainPersistentMemory();

            if (!l_val%2) {
                l_val++;
            } 
            else {
                l_val+=2;
            }
            *queue->Dflush[new_sp.struct_data.index/LOCAL_POOL_SIZE] = l_val;

            for (i = 0, prefix = 0; i < th_state->diffs_copy.tvec_cells; i++, prefix += _TVEC_BIWORD_SIZE_) {
                while (th_state->diffs_copy.cell[i] != 0L) {
                    register int pos, proc_id;
                    pos = synchBitSearchFirst(th_state->diffs_copy.cell[i]);
                    proc_id = prefix + pos;
                    th_state->diffs_copy.cell[i] ^= ((bitword_t)1) << pos;
                    queue->Dcomb_round[pid][proc_id] = l_val;
                }
            }            

            if (old_sp.raw_data == queue->Dpstate->S.raw_data && synchCAS64(&queue->Dpstate->S, old_sp.raw_data, new_sp.raw_data)) {                    // try to change stack->S to the value mod_dw
                synchFlushPersistentMemory((void *)&queue->Dpstate->S, sizeof(uint64_t));
                synchDrainPersistentMemory();
                synchCAS64(queue->Dflush[new_sp.struct_data.index/LOCAL_POOL_SIZE], l_val, l_val+1);
                th_state->backoff = (th_state->backoff >> 1) | 1;
                return lsp_data->return_val[pid];
            }
        } else if (th_state->backoff < queue->MAX_BACK) th_state->backoff <<= 1;
    }

    curr_pool_index = queue->Dpstate->S.struct_data.index;
    l_val = *queue->Dflush[curr_pool_index/LOCAL_POOL_SIZE];
    if (l_val%2 == 1 && l_val == queue->Dcomb_round[curr_pool_index/LOCAL_POOL_SIZE][pid]) {
        synchFlushPersistentMemory((void *)&queue->Dpstate->S, sizeof(uint64_t));
        synchDrainPersistentMemory();
        synchCAS64(queue->Dflush[curr_pool_index/LOCAL_POOL_SIZE], l_val, l_val+1);
    }


    return queue->DState[curr_pool_index]->return_val[pid];
}
