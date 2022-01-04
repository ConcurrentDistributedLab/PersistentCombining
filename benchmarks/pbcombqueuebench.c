#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <sched.h>

#include <config.h>
#include <primitives.h>
#include <fastrand.h>
#include <pool.h>
#include <threadtools.h>
#include <pbcombqueue.h>
#include <barrier.h>
#include <bench_args.h>

PBCombQueueStruct *queue_object CACHE_ALIGN;
int64_t d1 CACHE_ALIGN, d2;
SynchBarrier bar CACHE_ALIGN;
SynchBenchArgs bench_args CACHE_ALIGN;

inline static void *Execute(void* Arg) {
    PBCombQueueThreadState *th_state;
    long i, rnum;
    volatile int j;
    long id = (long) Arg;

    synchFastRandomSetSeed(id + 1);
    th_state = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(PBCombQueueThreadState));
    PBCombQueueThreadStateInit(queue_object, th_state, (int)id);

    synchBarrierWait(&bar);
    if (id == 0)
        d1 = synchGetTimeMillis();

    for (i = 0; i < bench_args.runs; i++) {
        // perform an enqueue operation
        PBCombQueueApplyEnqueue(queue_object, th_state, (ArgVal) i, id);
        rnum = synchFastRandomRange(1, bench_args.max_work);
        for (j = 0; j < rnum; j++)
            ; 
        // perform a dequeue operation
        PBCombQueueApplyDequeue(queue_object, th_state, id);
        rnum = synchFastRandomRange(1, bench_args.max_work);
        for (j = 0; j < rnum; j++)
            ;
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    synchParseArguments(&bench_args, argc, argv);
    queue_object = synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(PBCombQueueStruct));
    PBCombQueueInit(queue_object, bench_args.nthreads);   
    
    synchBarrierSet(&bar, bench_args.nthreads);
    synchStartThreadsN(bench_args.nthreads, Execute, bench_args.fibers_per_thread);
    synchJoinThreadsN(bench_args.nthreads - 1);
    d2 = synchGetTimeMillis();

    printf("time: %d (ms)\tthroughput: %.2f (millions ops/sec)\t", (int) (d2 - d1), 2 * bench_args.runs * bench_args.nthreads/(1000.0*(d2 - d1)));
    synchPrintStats(bench_args.nthreads, bench_args.total_runs);
#ifdef DEBUG
    fprintf(stderr, "DEBUG: Enqueue: Object state: %d\n", queue_object->enqueue_struct.counter);
    fprintf(stderr, "DEBUG: Enqueue: rounds: %d\n", queue_object->enqueue_struct.rounds);
    fprintf(stderr, "DEBUG: Dequeue: Object state: %d\n", queue_object->dequeue_struct.counter);
    fprintf(stderr, "DEBUG: Dequeue: rounds: %d\n\n", queue_object->dequeue_struct.rounds);

    Node *first = *((Node **)queue_object->dequeue_struct.pstate->last_state->state);
    long counter = 0;
    while (first->next != NULL) {
        first = (Node *)first->next;
        counter++;
    }
    fprintf(stderr, "DEBUG: %ld nodes were left in the queue\n", counter); // Do not count queue->guard node
#endif

    return 0;
}
