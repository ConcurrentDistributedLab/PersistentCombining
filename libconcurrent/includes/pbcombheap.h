/// @file pbcombheap.h
/// @authors Panagiota Fatourou and Nikolaos D. Kallimanis and Eleftherios Kosmas
/// @brief This file exposes the API of the PBheap, which is a persistent heap object of fixed size.
/// The provided implementation uses the serial heap implementation of fixed size provided by `heap.h` combined with PBcomb combining object 
/// provided by `pbcomb.h`. An example of use of this API is provided in benchmarks/pbcombheapbench.c file.
///
/// For a more detailed description see the original publication:
/// Panagiota Fatourou, Nikolaos D. Kallimanis, and Eleftherios Kosmas. "The Performance Power of Software Combining in Persistence".
/// ACM SIGPLAN Notices. Principles and Practice of Parallel Programming (PPoPP) 2022.
/// @copyright Copyright (c) 2021
#ifndef _PBCOMBHEAP_H_
#define _PBCOMBHEAP_H_

#include <limits.h>
#include <pbcomb.h>

#include <heap.h>

/// @brief PBCombHeapStruct stores the state of an instance of the PBheap persistent heap implementation.
/// PBCombHeapStruct should be initialized using the PBCombHeapStructInit function.
typedef struct PBCombHeapStruct {
    /// @brief An instance of PBcomb.
    PBCombStruct heap CACHE_ALIGN;
    /// @brief An instance of a serial heap implementation (see `heap.h`).
    HeapState initial_state; 
} PBCombHeapStruct;

/// @brief PBCombHeapThreadState stores each thread's local state for a single instance of PBheap.
/// For each instance of PBheap, a discrete instance of PBCombHeapThreadStateState should be used.
typedef struct PBCombHeapThreadState {
    /// @brief A PBCombThreadState struct for the instance of PBcomb.
    PBCombThreadState thread_state;
} PBCombHeapThreadState;

///  @brief This function initializes an instance of the PBheap persistent heap implementation.
///
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any operation on the heap object.
///  
///  @param heap_struct A pointer to an instance of the PBheap persistent heap implementation.
///  @param nthreads The number of threads that will use the PBheap persistent heap implementation.
void PBCombHeapInit(PBCombHeapStruct *heap_struct, uint32_t nthreads);

///  @brief This function should be called once by every thread before it applies any operation to the PBheap persistent heap implementation.
///  
///  @param heap_struct A pointer to an instance of the PBheap persistent heap implementation.
///  @param lobject_struct A pointer to thread's local state of PBheap.
///  @param pid The pid of the calling thread.
void PBCombHeapThreadStateInit(PBCombHeapStruct *heap_struct, PBCombHeapThreadState *lobject_struct, int pid);

///  @brief This function inserts a new element with value `arg` to the heap. 
///  
///  @param heap_struct A pointer to an instance of the PBheap persistent heap implementation.
///  @param lobject_struct A pointer to thread's local state of PBheap.
///  @param arg The value of the element that will be inserted in the heap.
///  @param pid The pid of the calling thread.
void PBCombHeapInsert(PBCombHeapStruct *heap_struct, PBCombHeapThreadState *lobject_struct, HeapElement arg, int pid);

///  @brief This function removes the element of the heap that has the minimum value.
///  
///  @param heap_struct A pointer to an instance of the PBheap persistent heap implementation.
///  @param lobject_struct A pointer to thread's local state of PBheap.
///  @param pid The pid of the calling thread.
///  @return The value of the removed element. In case that the heap is empty `EMPTY_HEAP` is returned.
HeapElement PBCombHeapDeleteMin(PBCombHeapStruct *heap_struct, PBCombHeapThreadState *lobject_struct, int pid);

///  @brief This function returns (without removing) the element of the heap that has the minimum value.
///  
///  @param heap_struct A pointer to an instance of the PBheap persistent heap implementation.
///  @param lobject_struct A pointer to thread's local state of PBheap.
///  @param pid The pid of the calling thread.
///  @return The value of the minimum element contained in the heap. In case that the heap is empty `EMPTY_HEAP` is returned. 
HeapElement PBCombHeapGetMin(PBCombHeapStruct *heap_struct, PBCombHeapThreadState *lobject_struct, int pid);

#endif