#include <pwfcomb.h>
#include <numa.h>

static inline void SimPersistentObjectStateCopy(PWFCombStateRec *dest, PWFCombStateRec *src);

static inline void SimPersistentObjectStateCopy(PWFCombStateRec *dest, PWFCombStateRec *src) {
    // copy everything except 'return_val', 'request, 'toggles', 'deactivate', and 'index' fields
    memcpy(&dest->st, &src->st, PWFCombObjectStateSize(dest->deactivate.nthreads) - 2 * sizeof(ToggleVector) - sizeof(RetVal *));
}

void PWFCombInit(PWFCombStruct *pwfcomb_struct, uint32_t nthreads, int max_backoff) {
    int i;

    pwfcomb_struct->nthreads = nthreads;
    for (i = 0; i < _SIM_PERSISTENT_FAD_DIVISIONS_; i++)
        TVEC_INIT_AT((ToggleVector *)&pwfcomb_struct->activate[i], nthreads, synchGetPersistentMemory(CACHE_LINE_SIZE, _TVEC_VECTOR_SIZE(nthreads)));
    
    pwfcomb_struct->request = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(PWFCombRequestRec));
    pwfcomb_struct->comb_round = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(uint64_t *));
    for (i = 0; i < nthreads; i++) {
        pwfcomb_struct->request[i].arg = 0;
        pwfcomb_struct->request[i].valid = false;
        pwfcomb_struct->comb_round[i] = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(uint64_t));
        int j;
        for (j = 0; j < nthreads; j++) {
            pwfcomb_struct->comb_round[i][j] = 0;
        }
    }
    pwfcomb_struct->mem_state = synchGetPersistentMemory(CACHE_LINE_SIZE, sizeof(PWFCombStateRec *) * (_SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads + 1));
    for (i = 0; i < _SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads + 1; i++) {
        void *p = synchGetPersistentMemory(CACHE_LINE_SIZE, PWFCombObjectStateSize(nthreads));
        pwfcomb_struct->mem_state[i] = p;
        TVEC_INIT_AT(&pwfcomb_struct->mem_state[i]->deactivate, nthreads, (void *)pwfcomb_struct->mem_state[i]->__flex);
        TVEC_INIT_AT(&pwfcomb_struct->mem_state[i]->index, nthreads, (void *)pwfcomb_struct->mem_state[i]->__flex + _TVEC_VECTOR_SIZE(nthreads));
        pwfcomb_struct->mem_state[i]->return_val = ((void *)pwfcomb_struct->mem_state[i]->__flex) + 2 * _TVEC_VECTOR_SIZE(nthreads);
    }

    pwfcomb_struct->flush = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(uint64_t *) * (nthreads + 1));
    for (i = 0; i < nthreads + 1; i++) {
        pwfcomb_struct->flush[i] = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(uint64_t));
    }

 
    pwfcomb_struct->S.struct_data.index = _SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads;
    pwfcomb_struct->S.struct_data.seq = 0;

    // OBJECT'S INITIAL VALUE
    // ----------------------
    *pwfcomb_struct->flush[nthreads] = 0;
    TVEC_SET_ZERO((ToggleVector *)&pwfcomb_struct->mem_state[_SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads]->deactivate);
    TVEC_SET_ZERO((ToggleVector *)&pwfcomb_struct->mem_state[_SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads]->index);
    pwfcomb_struct->MAX_BACK = max_backoff * 100;
#ifdef DEBUG
    pwfcomb_struct->mem_state[_SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads]->counter = 0;
    pwfcomb_struct->mem_state[_SIM_PERSISTENT_LOCAL_POOL_SIZE_ * nthreads]->rounds = 0;
#endif

    synchFullFence();
}

void PWFCombThreadStateInit(PWFCombThreadState *th_state, uint32_t nthreads, int pid) {
    TVEC_INIT(&th_state->mask, nthreads);
    TVEC_INIT(&th_state->index, nthreads);
    TVEC_INIT(&th_state->diffs, nthreads);
    TVEC_INIT(&th_state->l_activate, nthreads);
    TVEC_INIT(&th_state->diffs_copy, nthreads);

    TVEC_SET_BIT(&th_state->mask, pid);
    TVEC_NEGATIVE(&th_state->index, &th_state->mask);          //  1111011
    th_state->backoff = 1;
}

static __thread int fad_division = -1;

Object PWFCombApplyOp(PWFCombStruct *pwfcomb_struct, PWFCombThreadState *th_state, RetVal (*sfunc)(void *, ArgVal, int), Object arg, int pid) {
    ToggleVector *diffs = &th_state->diffs,
                 *l_activate = &th_state->l_activate;
    pointer_t old_sp, new_sp;
    PWFCombStateRec *sp_data, *lsp_data;
    int i, j, prefix, mybank;
    int curr_pool_index;
    uint64_t l_val;

    if (fad_division == -1) {
        fad_division = numa_node_of_cpu(synchGetPreferedCore()) % _SIM_PERSISTENT_FAD_DIVISIONS_;
        if (fad_division == -1) fad_division = 0;
    }

    pwfcomb_struct->request[pid].arg = arg;                                               // pwfcomb_struct->request the operation
    pwfcomb_struct->request[pid].valid = true;
    synchFullFence();

    mybank = TVEC_GET_BANK_OF_BIT(pid, pwfcomb_struct->nthreads);
    TVEC_NEGATIVE_BANK(&th_state->index, &th_state->index, mybank);
    TVEC_ATOMIC_ADD_BANK(&pwfcomb_struct->activate[fad_division], &th_state->index, mybank); // index pid's bit in pwfcomb_struct->activate, Fetch&Add acts as a full write-barrier
    
    if (!synchIsSystemOversubscribed()) {
        volatile int k;
        int backoff_limit;

        if (synchFastRandomRange(1, pwfcomb_struct->nthreads) > 1) { 
            backoff_limit = th_state->backoff;
            for (k = 0; k < backoff_limit; k++)
                ;
        }
    } else {
        if (synchFastRandomRange(1, pwfcomb_struct->nthreads) > 4)
            synchResched();
    }

    for (j = 0; j < 2; j++) {
        old_sp = pwfcomb_struct->S;                                                           // read reference to struct ObjectState
        sp_data = pwfcomb_struct->mem_state[old_sp.struct_data.index];                              // read reference of struct ObjectState in a local variable lsim_persistent_struct->S

        // Performance improvement
        TVEC_XOR_BANKS(diffs, &pwfcomb_struct->activate[fad_division], &sp_data->deactivate, mybank);                               // determine the set of active processes
        l_val = *pwfcomb_struct->flush[old_sp.struct_data.index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_]; 
        if (old_sp.raw_data != pwfcomb_struct->S.raw_data)
            continue;
        if (!TVEC_IS_SET(diffs, pid))                                                           // if the operation has already been deactivate return
            break;

        uint64_t local_index = pid * _SIM_PERSISTENT_LOCAL_POOL_SIZE_ + TVEC_IS_SET(&sp_data->index, pid);
        lsp_data = pwfcomb_struct->mem_state[local_index];
        SimPersistentObjectStateCopy(lsp_data, sp_data);
        if (old_sp.raw_data != pwfcomb_struct->S.raw_data)
            continue;

        TVEC_SET_ZERO(l_activate);
        for (i = 0; i < _SIM_PERSISTENT_FAD_DIVISIONS_; i++) {
            TVEC_OR(l_activate, l_activate, (ToggleVector *)&pwfcomb_struct->activate[i]);      // This is an atomic read, since a_toogles is volatile
        }

        TVEC_XOR(diffs, &lsp_data->deactivate, l_activate);
        if (!TVEC_IS_SET(diffs, pid))                                                           // if the operation has already been deactivate return
            break;
        
#ifdef DEBUG
        lsp_data->rounds++;
        lsp_data->counter++;
#endif
        lsp_data->return_val[pid] = sfunc(&lsp_data->st, arg, pid);      
        TVEC_COPY(&th_state->diffs_copy, diffs);
        TVEC_REVERSE_BIT(diffs, pid);
        for (i = 0, prefix = 0; i < diffs->tvec_cells; i++, prefix += _TVEC_BIWORD_SIZE_) {
            synchReadPrefetch(&pwfcomb_struct->request[prefix]);
            synchReadPrefetch(&pwfcomb_struct->request[prefix + 8]);
            synchReadPrefetch(&pwfcomb_struct->request[prefix + 16]);
            synchReadPrefetch(&pwfcomb_struct->request[prefix + 24]);

            while (diffs->cell[i] != 0L) {
                register int pos, proc_id;

                pos = synchBitSearchFirst(diffs->cell[i]);
                proc_id = prefix + pos;
                diffs->cell[i] ^= ((bitword_t)1) << pos;
                if (pwfcomb_struct->request[proc_id].valid == false) {
                    TVEC_REVERSE_BIT(l_activate, proc_id);
                    continue;
                }
                lsp_data->return_val[proc_id] = pwfcomb_struct->request[proc_id].arg;
                lsp_data->return_val[proc_id] = sfunc(&lsp_data->st, pwfcomb_struct->request[proc_id].arg, proc_id);
#ifdef DEBUG
                lsp_data->counter++;
#endif
            }
        }
        TVEC_COPY(&lsp_data->deactivate, l_activate);                                              // change deactivate to be equal to what was read in pwfcomb_struct->activate
        TVEC_REVERSE_BIT(&lsp_data->index, pid);

        new_sp.struct_data.seq = old_sp.struct_data.seq + 1;                                   // increase timestamp
        new_sp.struct_data.index = local_index;

        if (old_sp.raw_data==pwfcomb_struct->S.raw_data) {
            synchFlushPersistentMemory((void *)lsp_data, PWFCombObjectStateSize(pwfcomb_struct->nthreads));
            synchDrainPersistentMemory();

            if (!l_val%2) {
                l_val++;
            } 
            else {
                l_val+=2;
            }
            *pwfcomb_struct->flush[new_sp.struct_data.index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_] = l_val;


            for (i = 0, prefix = 0; i < th_state->diffs_copy.tvec_cells; i++, prefix += _TVEC_BIWORD_SIZE_) {
                while (th_state->diffs_copy.cell[i] != 0L) {
                    register int pos, proc_id;
                    pos = synchBitSearchFirst(th_state->diffs_copy.cell[i]);
                    proc_id = prefix + pos;
                    th_state->diffs_copy.cell[i] ^= ((bitword_t)1) << pos;
                    pwfcomb_struct->comb_round[pid][proc_id] = l_val;
                }
            }

            if (old_sp.raw_data==pwfcomb_struct->S.raw_data && synchCAS64(&pwfcomb_struct->S, old_sp.raw_data, new_sp.raw_data)) {                    // try to change pwfcomb_struct->S to the value mod_dw
                synchFlushPersistentMemory((void *)&pwfcomb_struct->S, sizeof(uint64_t));
                synchDrainPersistentMemory();
                synchCAS64(pwfcomb_struct->flush[new_sp.struct_data.index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_], l_val, l_val+1);
                th_state->backoff = (th_state->backoff >> 1) | 1;
                return lsp_data->return_val[pid];
            }
        } else if (th_state->backoff < pwfcomb_struct->MAX_BACK) th_state->backoff <<= 1;
    }

    curr_pool_index = pwfcomb_struct->S.struct_data.index;
    l_val = *pwfcomb_struct->flush[curr_pool_index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_];
    if (l_val%2 == 1 && l_val == pwfcomb_struct->comb_round[curr_pool_index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_][pid]) {
        synchFlushPersistentMemory((void *)&pwfcomb_struct->S, sizeof(uint64_t));
        synchDrainPersistentMemory();
        synchCAS64(pwfcomb_struct->flush[curr_pool_index/_SIM_PERSISTENT_LOCAL_POOL_SIZE_], l_val, l_val+1);
    }

    return pwfcomb_struct->mem_state[curr_pool_index]->return_val[pid];                                    // return the value found in the record stored there
}
