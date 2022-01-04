#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

#include <config.h>
#include <primitives.h>
#include <tvec.h>
#include <fastrand.h>
#include <pool.h>
#include <threadtools.h>
#include <pwfcombqueue.h>
#include <barrier.h>
#include <bench_args.h>

PWFCombQueueStruct *queue;
int64_t d1, d2;
SynchBarrier bar CACHE_ALIGN;
SynchBenchArgs bench_args CACHE_ALIGN;

static void *Execute(void* Arg) {
    PWFCombQueueThreadState *th_state;
    long i = 0;
    long id = (long) Arg;
    long rnum;
    volatile int j = 0;

    synchFastRandomSetSeed(id + 1);
    th_state = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(PWFCombQueueThreadState));
    PWFCombQueueThreadStateInit(queue, th_state, id);

    synchBarrierWait(&bar);
    if (id == 0)
        d1 = synchGetTimeMillis();
    for (i = 0; i < bench_args.runs; i++) {
        PWFCombQueueEnqueue(queue, th_state, id, id);
        rnum = synchFastRandomRange(1, bench_args.max_work);
        for (j = 0; j < rnum; j++)
            ;
        PWFCombQueueDequeue(queue, th_state, id);
        rnum = synchFastRandomRange(1, bench_args.max_work);
        for (j = 0; j < rnum; j++)
            ;
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    synchParseArguments(&bench_args, argc, argv);
    queue = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(PWFCombQueueStruct));
    PWFCombQueueInit(queue, bench_args.nthreads, bench_args.backoff_high);

    synchBarrierSet(&bar, bench_args.nthreads);
    synchStartThreadsN(bench_args.nthreads, Execute, bench_args.fibers_per_thread);
    synchJoinThreadsN(bench_args.nthreads - 1);
    d2 = synchGetTimeMillis();

    printf("time: %d (ms)\tthroughput: %.2f (millions ops/sec)\t", (int) (d2 - d1), 2 * bench_args.runs * bench_args.nthreads/(1000.0*(d2 - d1)));
    synchPrintStats(bench_args.nthreads, bench_args.total_runs);

#ifdef DEBUG
    Node *first = queue->EState[queue->Epstate->S.struct_data.index]->first;
    Node *last = queue->EState[queue->Epstate->S.struct_data.index]->last;
    if (first != NULL) {
        synchCASPTR(&first->next, NULL, last);
    }
    fprintf(stderr, "DEBUG: Enqueue: Object state: %ld\n", (long)queue->EState[queue->Epstate->S.struct_data.index]->counter);
    fprintf(stderr, "DEBUG: Dequeue: Object state: %ld\n", (long)queue->DState[queue->Dpstate->S.struct_data.index]->counter);
    volatile Node *head = queue->DState[queue->Dpstate->S.struct_data.index]->head;
    long counter = 0;
    while (head->next != NULL) {
        head = head->next;
        fprintf(stderr, "Node: %ld\n", head->val);
        counter++;
    }
    fprintf(stderr, "DEBUG: %ld nodes were left in the queue\n", counter); // Do not count queue->guard node
#endif

    return 0;
}
