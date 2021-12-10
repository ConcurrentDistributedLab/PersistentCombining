#include <heap.h>
#include <pbcombheap.h>

void PBCombHeapInit(PBCombHeapStruct *heap_struct, uint32_t nthreads) {
    heapInit(&heap_struct->initial_state);
    PBCombStructInit(&heap_struct->heap, nthreads, &heap_struct->initial_state, sizeof(HeapState));
}

void PBCombHeapThreadStateInit(PBCombHeapStruct *heap_struct, PBCombHeapThreadState *lobject_struct, int pid) {
    PBCombThreadStateInit(&heap_struct->heap, &lobject_struct->thread_state, pid);
}

void PBCombHeapInsert(PBCombHeapStruct *heap_struct, PBCombHeapThreadState *lobject_struct, HeapElement arg, int pid) {
    PBCombApplyOp(&heap_struct->heap, &lobject_struct->thread_state, heapSerialOperation, arg | _HEAP_INSERT_OP, pid);
}

HeapElement PBCombHeapDeleteMin(PBCombHeapStruct *heap_struct, PBCombHeapThreadState *lobject_struct, int pid) {
    return PBCombApplyOp(&heap_struct->heap, &lobject_struct->thread_state, heapSerialOperation, _HEAP_DELETE_MIN_OP, pid);
}

HeapElement PBCombHeapGetMin(PBCombHeapStruct *heap_struct, PBCombHeapThreadState *lobject_struct, int pid) {
    return PBCombApplyOp(&heap_struct->heap, &lobject_struct->thread_state, heapSerialOperation, _HEAP_GET_MIN_OP, pid);
}
