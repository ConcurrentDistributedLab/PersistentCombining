#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "config.h"
#include "primitives.h"
#include "pbcomb.h"
#include "threadtools.h"

#ifdef NUMA_SUPPORT
#   include <numa.h>
#endif

#define COMBINING_ROUNDS                 20

#ifdef NUMA_SUPPORT
int compare_numa(const void *A, const void *B) {
    static uint32_t ncores = 0;
    const uint32_t *n1 = A; 
    const uint32_t *n2 = B;
    
    int32_t prefered_core_n1 = preferedCoreOfThread(*n1);
    int32_t prefered_core_n2 = preferedCoreOfThread(*n2);

    if (ncores == 0)
        ncores = numa_num_configured_cpus();

    if (numa_node_of_cpu(0) == numa_node_of_cpu(ncores / 2)) {
        if (prefered_core_n1 >= ncores/2) {
            prefered_core_n1 -= ncores/2;
        }
        
        if (prefered_core_n2 >= ncores/2) {
            prefered_core_n2 -= ncores/2;
        }
    }
    
    return prefered_core_n1 - prefered_core_n2;
}
#endif

void PBCombStructInit(PBCombStruct *l, uint32_t nthreads, void *initial_state, uint32_t state_size) {
    int i;

    l->lock = 0;
#ifdef DEBUG
    l->counter = 0;
    l->rounds = 0;
#endif
    l->nthreads = nthreads;
    l->request = synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(PBCombRequest));

    for (i = 0; i < nthreads; i++) {
        l->request[i].arg = 0;
        l->request[i].activate = 0;
        l->request[i].valid = 0;
    }

    l->state_size = state_size;
    l->pstate = synchGetPersistentMemory(CACHE_LINE_SIZE, sizeof(PBCombPersistentState));
    l->pstate->last_state = synchGetPersistentMemory(CACHE_LINE_SIZE, sizeof(PBCombStateRec) + state_size +
                                        nthreads * sizeof(RetVal) + nthreads * sizeof(bool));
    l->pstate->last_state->state = ((void *)l->pstate->last_state->flex);
    l->pstate->last_state->return_value = ((void *)l->pstate->last_state->flex) + state_size;
    l->pstate->last_state->deactivate = ((void *)l->pstate->last_state->flex) + state_size + l->nthreads * sizeof(RetVal);

    memcpy((void *)l->pstate->last_state->state, initial_state, state_size);

    for (i = 0; i < l->nthreads; i++) {
        l->pstate->last_state->return_value[i] = 0;
        l->pstate->last_state->deactivate[i] = 0;
    }
    l->pstate->last_state->lock_value = 0;
#ifdef NUMA_SUPPORT
    l->numa_nodes = numa_num_task_nodes();
#else
    l->numa_nodes = 1;
#endif

    l->numa_ids = synchGetPersistentMemory(CACHE_LINE_SIZE, nthreads * sizeof(uint32_t));
    for (i = 0; i < nthreads; i++)
        l->numa_ids[i] = i;

#ifdef NUMA_SUPPORT
    qsort((void *)l->numa_ids, nthreads, sizeof(uint32_t), compare_numa);
#endif

    l->aux = NULL;
    l->final_persist_func = NULL;
    l->after_persist_func = NULL;
    synchFullFence();
}

void PBCombSetFinalPersist(PBCombStruct *l, void (*final_persist_func)(void *)) {
    l->final_persist_func = final_persist_func;
}

void PBCombSetAfterPersist(PBCombStruct *l, void (*after_persist_func)(void *)) {
    l->after_persist_func = after_persist_func;
}

void PBCombThreadStateInit(PBCombStruct *l, PBCombThreadState *st_thread, int pid) {
    int i;

#ifdef NUMA_SUPPORT
    for (i = 0; i < l->nthreads; i++) {
        if (l->numa_ids[i] == pid) {
            st_thread->numa_id = i;
            break;
        }
    }
#else
    st_thread->numa_node = 0;
    st_thread->numa_id = pid;
#endif

    st_thread->pool_index = 0;
    for (i = 0; i < PBCOMB_POOL_SIZE; i++) {
        st_thread->pool[i] = synchGetPersistentMemory(CACHE_LINE_SIZE, sizeof(PBCombStateRec) + l->state_size +
                                                 l->nthreads * sizeof(RetVal) + l->nthreads * sizeof(bool));
        st_thread->pool[i]->state = ((void *)st_thread->pool[i]->flex);
        st_thread->pool[i]->return_value = ((void *)st_thread->pool[i]->flex) + l->state_size;
        st_thread->pool[i]->deactivate = ((void *)st_thread->pool[i]->flex) + l->state_size + l->nthreads * sizeof(RetVal);
    }
}

RetVal PBCombApplyOp(PBCombStruct *s, PBCombThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), ArgVal arg, int pid) {
    int i, j;

    s->request[st_thread->numa_id].arg = arg;
    s->request[st_thread->numa_id].activate = 1 - s->request[st_thread->numa_id].activate;
    if (!s->request[st_thread->numa_id].valid) {
        s->request[st_thread->numa_id].valid = 1;
    }
    synchFullFence();

    while (true) {
        int32_t lock_value = s->lock;

        if (lock_value % 2 == 0) {
            if (synchCAS32(&s->lock, lock_value, lock_value + 1)) {
                break;
            }
            lock_value++;
        } else {
            while(s->lock == lock_value)
                synchResched();

            volatile PBCombStateRec *last_state = s->pstate->last_state;
            if (last_state->deactivate[st_thread->numa_id] == s->request[st_thread->numa_id].activate) {
                if (last_state->lock_value == lock_value)
                    return last_state->return_value[st_thread->numa_id];
                while (s->lock == lock_value+2)
                    synchResched();
                return last_state->return_value[st_thread->numa_id];
            }
        }
    }

#ifdef DEBUG
     s->rounds += 1;
#endif
    volatile PBCombStateRec *new_state = st_thread->pool[st_thread->pool_index];
    memcpy((void *)new_state->flex, (void *)s->pstate->last_state->flex, 
           s->state_size + s->nthreads * sizeof(RetVal) + s->nthreads * sizeof(bool));

    for (i=0; i < COMBINING_ROUNDS; i++) {
        uint64_t serve_reqs = 0;

        for (j = 0; j < s->nthreads; j++) {
            if (new_state->deactivate[j] != s->request[j].activate && s->request[j].valid == 1) {
                new_state->return_value[j] = sfunc((void *)new_state->state, s->request[j].arg, j);
                new_state->deactivate[j] = s->request[j].activate;
                serve_reqs++;
#ifdef DEBUG
                s->counter += 1;
#endif
            }
        }
        if (serve_reqs == 0)
            break;
    }

    if (s->final_persist_func != NULL) {
        s->final_persist_func((void *)s);
    }

    synchFlushPersistentMemory((void *)new_state->flex, s->state_size + s->nthreads * sizeof(RetVal) + s->nthreads * sizeof(bool));
    synchDrainPersistentMemory();

    s->pstate->last_state = new_state;

    synchFlushPersistentMemory((void *)s->pstate, sizeof(PBCombPersistentState));
    synchDrainPersistentMemory();

    if (s->after_persist_func != NULL) {
        s->after_persist_func((void *)s);
    }

    st_thread->pool_index = (st_thread->pool_index + 1) % PBCOMB_POOL_SIZE;
    s->pstate->last_state->lock_value = s->lock;
    s->lock += 1;
    synchFullFence();

    return new_state->return_value[st_thread->numa_id];
}
