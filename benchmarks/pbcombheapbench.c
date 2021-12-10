#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>

#include <config.h>
#include <primitives.h>
#include <fastrand.h>
#include <threadtools.h>
#include <pbcombheap.h>
#include <barrier.h>
#include <bench_args.h>

PBCombHeapStruct *object_struct CACHE_ALIGN;
int64_t d1 CACHE_ALIGN, d2;
SynchBarrier bar CACHE_ALIGN;
SynchBenchArgs bench_args CACHE_ALIGN;

inline static void *Execute(void* Arg) {
    PBCombHeapThreadState *th_state;
    long i, rnum;
    volatile int j;
    long id = (long) Arg;

    synchFastRandomSetSeed(id + 1);
    th_state = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(PBCombHeapThreadState));
    PBCombHeapThreadStateInit(object_struct, th_state, (int)id);
    synchBarrierWait(&bar);
    if (id == 0)
        d1 = synchGetTimeMillis();

    for (i = 0; i < bench_args.runs; i++) {
        // perform a push operation
        PBCombHeapInsert(object_struct, th_state, bench_args.runs - i, id);
        rnum = synchFastRandomRange(1, bench_args.max_work);
        for (j = 0; j < rnum; j++)
            ; 
        // perform a pop operation
        PBCombHeapDeleteMin(object_struct, th_state, id);
        rnum = synchFastRandomRange(1, bench_args.max_work);
        for (j = 0; j < rnum; j++)
            ;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    PBCombHeapThreadState th_state;
    int i;

    synchParseArguments(&bench_args, argc, argv);
    object_struct = synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(PBCombHeapStruct));
    PBCombHeapInit(object_struct, bench_args.nthreads);
    PBCombHeapThreadStateInit(object_struct, &th_state, 0);
    for (i = 0; i < INITIAL_HEAP_SIZE/2; i++)
        PBCombHeapInsert(object_struct, &th_state, i, 0);

    synchBarrierSet(&bar, bench_args.nthreads);
    synchStartThreadsN(bench_args.nthreads, Execute, bench_args.fibers_per_thread);
    synchJoinThreadsN(bench_args.nthreads - 1);
    d2 = synchGetTimeMillis();

    printf("time: %d (ms)\tthroughput: %.2f (millions ops/sec)\t", (int) (d2 - d1), 2 * bench_args.runs * bench_args.nthreads/(1000.0*(d2 - d1)));
    synchPrintStats(bench_args.nthreads, bench_args.total_runs);

#ifdef DEBUG
    fprintf(stderr, "DEBUG: object state: counter: %lld rounds: %d\n", object_struct->heap.counter - INITIAL_HEAP_SIZE, object_struct->heap.rounds);
#endif

    return 0;
}
