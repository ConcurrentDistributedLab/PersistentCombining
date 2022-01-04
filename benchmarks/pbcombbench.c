#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>

#include <config.h>
#include <primitives.h>
#include <fastrand.h>
#include <threadtools.h>
#include <barrier.h>
#include <pbcomb.h>
#include <bench_args.h>


volatile Object *object CACHE_ALIGN;
PBCombStruct *object_lock;
int64_t d1 CACHE_ALIGN, d2;
SynchBarrier bar CACHE_ALIGN;
SynchBenchArgs bench_args CACHE_ALIGN;


inline static RetVal fetchAndMultiply(void *state, ArgVal arg, int pid) {
    *object += arg;
    return *object;
}

inline static void *Execute(void* Arg) {
    PBCombThreadState lobject_lock;
    long i, rnum;
    volatile int j;
    long id = (long) Arg;

    synchFastRandomSetSeed(id + 1L);
    PBCombThreadStateInit(object_lock, &lobject_lock, (int)id);
    synchBarrierWait(&bar);
    if (id == 0)
        d1 = synchGetTimeMillis();
    for (i = 0; i < bench_args.runs; i++) {
        // perform a fetchAndMultiply operation
        PBCombApplyOp(object_lock, &lobject_lock, fetchAndMultiply, (ArgVal) id, id);
        rnum = synchFastRandomRange(1, bench_args.max_work);
        for (j = 0; j < rnum; j++)
            ;
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    synchParseArguments(&bench_args, argc, argv);

    object = synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(Object));
    *object = 1;
    object_lock = synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(PBCombStruct));
    PBCombStructInit(object_lock, bench_args.nthreads, (void *)object, sizeof(Object));

    synchBarrierSet(&bar, bench_args.nthreads);
    synchStartThreadsN(bench_args.nthreads, Execute, bench_args.fibers_per_thread);
    synchJoinThreadsN(bench_args.nthreads - 1);
    d2 = synchGetTimeMillis();

    printf("time: %d (ms)\tthroughput: %.2f (millions ops/sec)\t", (int) (d2 - d1), bench_args.runs * bench_args.nthreads/(1000.0*(d2 - d1)));
    synchPrintStats(bench_args.nthreads, bench_args.total_runs);

#ifdef DEBUG
    fprintf(stderr, "DEBUG: Object state: %d\n", object_lock->counter);
    fprintf(stderr, "DEBUG: rounds: %d\n", object_lock->rounds);
    fprintf(stderr, "DEBUG: Average helping: %f\n", (float)object_lock->counter/object_lock->rounds);
    fprintf(stderr, "\n");
#endif
    
    return 0;
}



