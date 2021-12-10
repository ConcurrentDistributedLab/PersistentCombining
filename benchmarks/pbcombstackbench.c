#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>

#include <config.h>
#include <primitives.h>
#include <fastrand.h>
#include <threadtools.h>
#include <pbcombstack.h>
#include <barrier.h>
#include <bench_args.h>

PBCombStackStruct *object_struct CACHE_ALIGN;
int64_t d1 CACHE_ALIGN, d2;
SynchBarrier bar CACHE_ALIGN;
SynchBenchArgs bench_args CACHE_ALIGN;

inline static void *Execute(void* Arg) {
    PBCombStackThreadState *th_state;
    long i, rnum;
    volatile int j;
    long id = (long) Arg;

    synchFastRandomSetSeed(id + 1);
    th_state = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(PBCombStackThreadState));
    PBCombStackThreadStateInit(object_struct, th_state, (int)id);

    synchBarrierWait(&bar);
    if (id == 0)
        d1 = synchGetTimeMillis();

    for (i = 0; i < bench_args.runs; i++) {
        // perform a push operation
        PBCombStackPush(object_struct, th_state, id, id);
        rnum = synchFastRandomRange(1, bench_args.max_work);
        for (j = 0; j < rnum; j++)
            ; 
        // perform a pop operation
        PBCombStackPop(object_struct, th_state, id);
        rnum = synchFastRandomRange(1, bench_args.max_work);
        for (j = 0; j < rnum; j++)
            ;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    synchParseArguments(&bench_args, argc, argv);
    object_struct = synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(PBCombStackStruct));
    PBCombStackInit(object_struct, bench_args.nthreads);
    synchBarrierSet(&bar, bench_args.nthreads);
    synchStartThreadsN(bench_args.nthreads, Execute, bench_args.fibers_per_thread);
    synchJoinThreadsN(bench_args.nthreads - 1);
    d2 = synchGetTimeMillis();

    printf("time: %d (ms)\tthroughput: %.2f (millions ops/sec)\t", (int) (d2 - d1), 2 * bench_args.runs * bench_args.nthreads/(1000.0*(d2 - d1)));
    synchPrintStats(bench_args.nthreads, bench_args.total_runs);

#ifdef DEBUG
    fprintf(stderr, "DEBUG: Object state: %d\n", object_struct->object_struct.counter);
    fprintf(stderr, "DEBUG: rounds: %d\n", object_struct->object_struct.rounds);

    volatile Node *head = *((Node **)object_struct->object_struct.pstate->last_state->state);
    long counter = 0;
    while (head != NULL) {
        head = (Node *)head->next;
        counter++;
    }
    fprintf(stderr, "DEBUG: %ld nodes were left in the queue\n", counter);
#endif

    return 0;
}
