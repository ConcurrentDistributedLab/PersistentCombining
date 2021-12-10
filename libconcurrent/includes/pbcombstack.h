/// @file pbcombstack.h
/// @authors Panagiota Fatourou and Nikolaos D. Kallimanis and Eleftherios Kosmas
/// @brief This file exposes the API of the PBstack, which is a persistent stack implementation.
/// An example of use of this API is provided in benchmarks/pbcombstackbench.c file.
///
/// For a more detailed description see the original publication:
/// Panagiota Fatourou, Nikolaos D. Kallimanis, and Eleftherios Kosmas. "The Performance Power of Software Combining in Persistence".
/// ACM SIGPLAN Notices. Principles and Practice of Parallel Programming (PPoPP) 2022.
///  @copyright Copyright (c) 2021
#ifndef _PERSTACK_H_
#define _PERSTACK_H_

#include <pbcomb.h>
#include <config.h>
#include <primitives.h>
#include <pool.h>
#include <queue-stack.h>

/// @brief PBCombStackStruct stores the state of an instance of the PBstack concurrent stack implementation.
/// PBCombStackStruct should be initialized using the PBCombStackStructInit function.
typedef struct PBCombStackStruct {
    /// @brief A PBcomb instance for servicing both push and pop operations.
    PBCombStruct object_struct CACHE_ALIGN;
    /// @brief A pointer to the head element of the stack.
    volatile Node * volatile head CACHE_ALIGN;
    /// @brief A pool of nodes that used by the combiner for massive and efficient node alocations.
    SynchPoolStruct pool_node CACHE_ALIGN;
} PBCombStackStruct;

/// @brief PBCombStackThreadState stores each thread's local state for a single instance of PBstack.
/// For each instance of PBstack, a discrete instance of PBCombStackThreadState should be used.
typedef struct PBCombStackThreadState {
    /// @brief A PBCombStackThreadState struct for the instance of PBcomb that serves both push and pop operations.
    PBCombThreadState th_state;
} PBCombStackThreadState;

///  @brief This function initializes an instance of the PBstack persistent stack implementation.
///
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any push or pop operation.
///  
/// @param stack_object_struct A pointer to an instance of the PBstack persistent stack implementation.
/// @param nthreads The number of threads that will use the PBstack persistent stack implementation.
void PBCombStackInit(PBCombStackStruct *stack_object_struct, uint32_t nthreads);

/// @brief This function should be called once by each thread before it applies any operation to the PBstack persistent stack implementation.
///
/// @param object_struct A pointer to an instance of the PBstack persistent stack implementation.
/// @param lobject_struct A pointer to thread's local state of PBstack.
/// @param pid The pid of the calling thread.
void PBCombStackThreadStateInit(PBCombStackStruct *object_struct, PBCombStackThreadState *lobject_struct, int pid);

/// @brief This function adds (i.e. pushes) a new element to the top of the stack.
/// This element has a value equal with arg.
///
/// @param object_struct A pointer to an instance of the PBstack persistent stack implementation.
/// @param lobject_struct A pointer to thread's local state of PBstack.
/// @param arg The push operation will insert a new element to the stack with value equal to arg.
/// @param pid The pid of the calling thread.
void PBCombStackPush(PBCombStackStruct *object_struct, PBCombStackThreadState *lobject_struct, ArgVal arg, int pid);

/// @brief This function removes (i.e. pops) an element from the top of the stack and returns its value.
///
/// @param object_struct A pointer to an instance of the PBstack persistent stack implementation.
/// @param lobject_struct A pointer to thread's local state of PBstack.
/// @param pid The pid of the calling thread.
/// @return The value of the removed element. 
RetVal PBCombStackPop(PBCombStackStruct *object_struct, PBCombStackThreadState *lobject_struct, int pid);

#endif
