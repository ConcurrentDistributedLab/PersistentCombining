#include <pwfcombstack.h>
#include <numa.h>

static const int POP = INT_MIN;
static const uint64_t NVMEM_CACHE_LINE_SIZE = 64;
static const uint64_t NEG_NVMEM_CACHE_LINE_SIZE = ~(64 - 1);

static __thread int fad_division = -1;
static __thread Node **clNewItems;
static __thread uint64_t *clNewItems_count;
static __thread uint64_t clNewItems_size = 0;

static inline void PWFCombStackStateCopy(PWFCombStackRec *dest, PWFCombStackRec *src);
inline static Node *serialPush(PWFCombStackRec *st, PWFCombStackThreadState *th_state, ArgVal arg);
inline static bool serialPop(PWFCombStackRec *st, int pid);
inline static void recycleList(SynchPoolStruct *pool, Node *head, uint32_t items);

inline static Node *serialPush(PWFCombStackRec *st, PWFCombStackThreadState *th_state, ArgVal arg) {
#ifdef DEBUG
    st->counter += 1;
#endif
    Node *n;
    n = synchAllocObj(&th_state->pool);
    n->val = (ArgVal)arg;
    n->next = st->head;
    st->head = n;

    return n;
}

inline static bool serialPop(PWFCombStackRec *st, int pid) {
    int i;
    uint64_t new_item_ptr;
#ifdef DEBUG
    st->counter += 1;
#endif
    if (st->head != NULL) {
        new_item_ptr = ((uint64_t)st->head) & NEG_NVMEM_CACHE_LINE_SIZE;
        for (i = 0; i < clNewItems_size; i++) {
            if (new_item_ptr == (uint64_t)clNewItems[i]) {
                clNewItems_count[i]--;
                break;
            }
        }

        st->return_val[pid] = (RetVal)st->head->val;
        st->head = (Node *)st->head->next;
        return true;
    } else {
        st->return_val[pid] = (RetVal)-1;
        return false;
    }
}

static inline void PWFCombStackStateCopy(PWFCombStackRec *dest, PWFCombStackRec *src) {
    // copy everything except 'return_val', 'request, 'toggles', 'deactivate', and 'index' fields
    memcpy(&dest->head, &src->head, PWFCombStackStateSize(dest->deactivate.nthreads) - CACHE_LINE_SIZE);
}

void PWFCombStackInit(PWFCombStackStruct *stack, uint32_t nthreads, int max_backoff) {
    int i;

    stack->nthreads = nthreads;
    for (i = 0; i < _SIM_PERSISTENT_FAD_DIVISIONS_; i++)
        TVEC_INIT_AT((ToggleVector *)&stack->activate[i], nthreads, synchGetPersistentMemory(CACHE_LINE_SIZE, _TVEC_VECTOR_SIZE(nthreads)));
    
    stack->request = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(PWFCombRequestRec));
    stack->comb_round = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(uint64_t *));

    for (i = 0; i < nthreads; i++) {
        stack->request[i].arg = 0;
        stack->request[i].valid = false;
        stack->comb_round[i] = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(uint64_t));
        int j;
        for (j = 0; j < nthreads; j++) {
            stack->comb_round[i][j] = 0;
        }

    }
    stack->mem_state = synchGetPersistentMemory(CACHE_LINE_SIZE, sizeof(PWFCombStackRec *) * (_SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads + 1));
    
    for (i = 0; i < _SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads + 1; i++) {
        void *p = synchGetPersistentMemory(CACHE_LINE_SIZE, PWFCombStackStateSize(nthreads));
        stack->mem_state[i] = p;
        TVEC_INIT_AT(&stack->mem_state[i]->deactivate, nthreads, (void *)stack->mem_state[i]->__flex);
        TVEC_INIT_AT(&stack->mem_state[i]->index, nthreads, (void *)stack->mem_state[i]->__flex + _TVEC_VECTOR_SIZE(nthreads));
        stack->mem_state[i]->return_val = ((void *)stack->mem_state[i]->__flex) + 2 * _TVEC_VECTOR_SIZE(nthreads);
    }

    stack->flush = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(uint64_t *) * (nthreads + 1));
    for (i = 0; i < nthreads + 1; i++) {
        stack->flush[i] = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(uint64_t));
    }

    stack->S.struct_data.index = _SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads;
    stack->S.struct_data.seq = 0;

    // OBJECT'S INITIAL VALUE
    // ----------------------
    *stack->flush[nthreads] = 0;
    TVEC_SET_ZERO((ToggleVector *)&stack->mem_state[_SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads]->deactivate);
    TVEC_SET_ZERO((ToggleVector *)&stack->mem_state[_SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads]->index);
    stack->MAX_BACK = max_backoff * 100;
#ifdef DEBUG
    stack->mem_state[_SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads]->counter = 0;
#endif
    synchFullFence();
}

void PWFCombStackThreadStateInit(PWFCombStackStruct *stack, PWFCombStackThreadState *th_state, uint32_t nthreads, int pid) {
    int i;

    TVEC_INIT(&th_state->mask, nthreads);
    TVEC_INIT(&th_state->index, nthreads);
    TVEC_INIT(&th_state->diffs, nthreads);
    TVEC_INIT(&th_state->l_activate, nthreads);
    TVEC_INIT(&th_state->pops, nthreads);
    TVEC_INIT(&th_state->diffs_copy, nthreads);

    TVEC_SET_BIT(&th_state->mask, pid);
    TVEC_NEGATIVE(&th_state->index, &th_state->mask);          //  1111011
    th_state->backoff = 1;
    synchInitPoolPersistent(&th_state->pool, sizeof(Node));

    clNewItems = synchGetAlignedMemory(CACHE_LINE_SIZE, stack->nthreads * sizeof(Node **));
    clNewItems_count = synchGetAlignedMemory(CACHE_LINE_SIZE, stack->nthreads * sizeof(uint64_t));
    for (i = 0; i < stack->nthreads; i++) {
        clNewItems[i] = NULL;
        clNewItems_count[i] = 0;
    }
    clNewItems_size = 0;
}

inline static void recycleList(SynchPoolStruct *pool, Node *head, uint32_t items) {
    while (items > 0) {
        Node *node = head;
        head = (Node *)head->next;
        items--;
        synchRecycleObj(pool, node);
    }
}

Object PWFCombStackApplyOp(PWFCombStackStruct *stack, PWFCombStackThreadState *th_state, Object arg, int pid) {
    ToggleVector *diffs = &th_state->diffs, *l_activate = &th_state->l_activate, *pops = &th_state->pops;
    pointer_t old_sp, new_sp;
    PWFCombStackRec *sp_data, *lsp_data;
    int i, j, prefix, mybank, push_counter;
    int curr_pool_index;
    uint64_t l_val;

    if (fad_division == -1) {
        fad_division = numa_node_of_cpu(synchGetPreferedCore()) % _SIM_PERSISTENT_FAD_DIVISIONS_;
        if (fad_division == -1) fad_division = 0;
    }
    stack->request[pid].arg = arg;                                               // stack->request the operation
    stack->request[pid].valid = true;
    synchFullFence();

    mybank = TVEC_GET_BANK_OF_BIT(pid, stack->nthreads);
    TVEC_NEGATIVE_BANK(&th_state->index, &th_state->index, mybank);
    TVEC_ATOMIC_ADD_BANK(&stack->activate[fad_division], &th_state->index, mybank); // toggle pid's bit in stack->activate, Fetch&Add acts as a full write-barrier
    
    if (!synchIsSystemOversubscribed()) {
        volatile int k;
        int backoff_limit;

        if (synchFastRandomRange(1, stack->nthreads) > 1) { 
            backoff_limit = th_state->backoff;
            for (k = 0; k < backoff_limit; k++)
                ;
        }
    } else {
        if (synchFastRandomRange(1, stack->nthreads) > 4)
            synchResched();
    }

    for (j = 0; j < 2; j++) {
        clNewItems_size = 0;
        for (i = 0; i < stack->nthreads; i++) {
            clNewItems_count[i] = 0;
        }
        old_sp = stack->S;                                                           // read reference to struct ObjectState
        sp_data = stack->mem_state[old_sp.struct_data.index];                              // read reference of struct ObjectState in a local variable lsim_persistent_struct->S
        TVEC_XOR_BANKS(diffs, &stack->activate[fad_division], &sp_data->deactivate, mybank);                               // determine the set of active processes
        l_val = *stack->flush[old_sp.struct_data.index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_]; 
        if (old_sp.raw_data != stack->S.raw_data)
            continue;
        if (!TVEC_IS_SET(diffs, pid))                                                           // if the operation has already been deactivate return
            break;
        
        uint64_t local_index = pid * _SIM_PERSISTENT_LOCAL_POOL_SIZE_ + TVEC_IS_SET(&sp_data->index, pid);
        lsp_data = stack->mem_state[local_index];
        PWFCombStackStateCopy(lsp_data, sp_data);
        
        TVEC_SET_ZERO(l_activate);
        for (i = 0; i < _SIM_PERSISTENT_FAD_DIVISIONS_; i++) {
            TVEC_OR(l_activate, l_activate, (ToggleVector *)&stack->activate[i]);      // This is an atomic read, since a_toogles is volatile
        }
        
        if (old_sp.raw_data != stack->S.raw_data)
            continue;
        
        TVEC_XOR(diffs, &lsp_data->deactivate, l_activate);
        TVEC_COPY(&th_state->diffs_copy, diffs);
        push_counter = 0;
        TVEC_SET_ZERO(pops);
        for (i = 0, prefix = 0; i < diffs->tvec_cells; i++, prefix += _TVEC_BIWORD_SIZE_) {
            synchReadPrefetch(&stack->request[prefix]);
            synchReadPrefetch(&stack->request[prefix + 8]);
            synchReadPrefetch(&stack->request[prefix + 16]);
            synchReadPrefetch(&stack->request[prefix + 24]);

            while (diffs->cell[i] != 0L) {
                register int pos, proc_id;

                pos = synchBitSearchFirst(diffs->cell[i]);
                proc_id = prefix + pos;
                diffs->cell[i] ^= ((bitword_t)1) << pos;

                if (stack->request[proc_id].arg == POP) {
                    pops->cell[i] |= ((bitword_t)1) << pos;
                } else {
                    if (stack->request[proc_id].valid == false) {
                        TVEC_REVERSE_BIT(l_activate, proc_id);
                        continue;
                    }
                    Node *node = serialPush(lsp_data, th_state, stack->request[proc_id].arg);
                    uint64_t new_item_ptr = ((uint64_t)node) & NEG_NVMEM_CACHE_LINE_SIZE;
                    bool found = false;
                    int k;

                    for (k = 0; k < clNewItems_size; k++) {
                        if (new_item_ptr == (uint64_t)clNewItems[k]) {
                            found = true;
                            clNewItems_count[i]++;
                            break;
                        }
                    }
                    if (!found) {
                        clNewItems[clNewItems_size] = (void *)(new_item_ptr);
                        clNewItems_count[clNewItems_size] = 1;
                        clNewItems_size++;
                    }
                    push_counter++;
                }
            }
        }

        Node *free_list = lsp_data->head;
        int pop_counter = 0;
        for (i = 0, prefix = 0; i < pops->tvec_cells; i++, prefix += _TVEC_BIWORD_SIZE_) {
            while (pops->cell[i] != 0L) {
                register int pos, proc_id;

                pos = synchBitSearchFirst(pops->cell[i]);
                proc_id = prefix + pos;
                pops->cell[i] ^= ((bitword_t)1) << pos;
                if (stack->request[proc_id].valid == false) {
                    TVEC_REVERSE_BIT(l_activate, proc_id);
                    continue;
                }
                pop_counter += serialPop(lsp_data, proc_id);
                if (old_sp.raw_data != stack->S.raw_data)
                    goto outer;
            }
        }
#ifdef SYNCH_DISABLE_ELIMINATION_ON_STACKS
        // Do not eliminate the PWB operations
        for (i = 0; i < clNewItems_size; i++) {
            if (old_sp.raw_data != stack->S.raw_data)
                break;
            synchFlushPersistentMemory((void *)clNewItems[i], NVMEM_CACHE_LINE_SIZE);
        }
#else
        // Trying to eliminate the PWB operations
        for (i = 0; i < clNewItems_size; i++) {
            if (old_sp.raw_data != stack->S.raw_data)
                break;
            if (clNewItems_count[i] > 0)
                synchFlushPersistentMemory((void *)clNewItems[i], NVMEM_CACHE_LINE_SIZE);
        }
#endif

        TVEC_COPY(&lsp_data->deactivate, l_activate);                                              // change deactivate to be equal to what was read in stack->activate
        TVEC_REVERSE_BIT(&lsp_data->index, pid);
outer:
        new_sp.struct_data.seq = old_sp.struct_data.seq + 1;                                   // increase timestamp
        new_sp.struct_data.index = local_index;                                                // store in mod_dw.index the index in stack->mem_state where lsim_persistent_struct->S will be stored


        if (old_sp.raw_data==stack->S.raw_data) {
            synchFlushPersistentMemory((void *)lsp_data, PWFCombObjectStateSize(stack->nthreads));
            synchDrainPersistentMemory();

            if (!l_val%2) { 
                l_val++;
            } 
            else {
                l_val+=2;
            }
            *stack->flush[new_sp.struct_data.index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_] = l_val;

            for (i = 0, prefix = 0; i < th_state->diffs_copy.tvec_cells; i++, prefix += _TVEC_BIWORD_SIZE_) {
                while (th_state->diffs_copy.cell[i] != 0L) {
                    register int pos, proc_id;
                    pos = synchBitSearchFirst(th_state->diffs_copy.cell[i]);
                    proc_id = prefix + pos;
                    th_state->diffs_copy.cell[i] ^= ((bitword_t)1) << pos;
                    stack->comb_round[pid][proc_id] = l_val;
                }
            }

            if (old_sp.raw_data==stack->S.raw_data && synchCAS64(&stack->S, old_sp.raw_data, new_sp.raw_data)) {                    // try to change stack->S to the value mod_dw
                synchFlushPersistentMemory((void *)&stack->S, sizeof(uint64_t));
                synchDrainPersistentMemory();
                synchCAS64(stack->flush[new_sp.struct_data.index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_], l_val, l_val+1);
                th_state->backoff = (th_state->backoff >> 1) | 1;
                recycleList(&th_state->pool, free_list, pop_counter);

                return lsp_data->return_val[pid];
            }
        } else {
            if (th_state->backoff < stack->MAX_BACK)
                th_state->backoff <<= 1;
            recycleList(&th_state->pool, free_list, push_counter);
        }
    }

    curr_pool_index = stack->S.struct_data.index;
    l_val = *stack->flush[curr_pool_index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_];
    if (l_val%2 == 1 && l_val == stack->comb_round[curr_pool_index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_][pid]) {
        synchFlushPersistentMemory((void *)&stack->S, sizeof(uint64_t));
        synchDrainPersistentMemory();
        synchCAS64(stack->flush[curr_pool_index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_], l_val, l_val+1);
    }

    return stack->mem_state[curr_pool_index]->return_val[pid];                                    // return the value found in the record stored there
}


void PWFCombStackPush(PWFCombStackStruct *stack, PWFCombStackThreadState *th_state, ArgVal arg, int pid) {
    PWFCombStackApplyOp(stack, th_state, arg, pid);
}

RetVal PWFCombStackPop(PWFCombStackStruct *stack, PWFCombStackThreadState *th_state, int pid) {
    return PWFCombStackApplyOp(stack, th_state, POP, pid);
}