/// @file pwfcombstack.h
/// @authors Panagiota Fatourou and Nikolaos D. Kallimanis and Eleftherios Kosmas
/// @brief This file exposes the API of the PWFstack, which is a persistent wait-free stack object.
/// An example of use of this API is provided in benchmarks/pwfcombstackbench.c file.
///
/// For a more detailed description see the original publication:
/// Panagiota Fatourou, Nikolaos D. Kallimanis, and Eleftherios Kosmas. "The Performance Power of Software Combining in Persistence".
/// ACM SIGPLAN Notices. Principles and Practice of Parallel Programming (PPoPP) 2022.
///  @copyright Copyright (c) 2021
#ifndef _PWFCOMBSTACK_H_
#define _PWFCOMBSTACK_H_

#include <config.h>
#include <primitives.h>
#include <pool.h>
#include <queue-stack.h>
#include <uthreads.h>
#include <pwfcomb.h>

/// @brief This struct stores the data for a copy of the persistent stack's state.
typedef struct PWFCombStackRec {
    /// @brief A vector of toggles, one per running thread. This toggle indicates if the corresponding running thread has a peding request or not.
    ToggleVector deactivate;
    ToggleVector index;
    /// @brief A pointer to the array of return values.
    Object *return_val;
    /// @brief The head pointer that points to most-top element of the stack.
    Node *head CACHE_ALIGN;
#ifdef DEBUG
    int counter;
#endif
    uint64_t __flex[1];
} PWFCombStackRec;

/// @brief A macro for calculating the size of the PWFCombStackRec struct for a specific amount of threads.
#define PWFCombStackStateSize(nthreads) (sizeof(PWFCombStackRec) + 2 * _TVEC_VECTOR_SIZE(nthreads) + nthreads * sizeof(Object) + sizeof(Node *))

/// @brief PWFCombStackThreadState stores each thread's local state for a single instance of PWFstack.
/// For each instance of PWFstack, a discrete instance of PWFCombStackThreadState should be used.
typedef struct PWFCombStackThreadState {
    SynchPoolStruct pool;
    ToggleVector mask;
    ToggleVector index;
    ToggleVector diffs;
    ToggleVector diffs_copy;
    ToggleVector l_activate;
    ToggleVector pops;
    int local_index;
    /// @brief Current backoff value.
    int backoff;
} PWFCombStackThreadState;

/// @brief PWFCombStackStruct stores the state of an instance of the a PWFstack persistent stack object.
/// PWFCombStackStruct should be initialized using the PWFCombStackStructInit function.
typedef struct PWFCombStackStruct {
    /// @brief Index (i.e. pointer) to a PWFCombStackRec structs that contains the most recent and valid copy of stack's state.
    volatile pointer_t S;
    /// @brief A vector of toggle bits, one toggle per NUMA node. The object could also work fine with a single such toggle.
    /// However, by using one toggle per NUMA node, the performance is increased substantially.
    ToggleVector activate[_SIM_PERSISTENT_FAD_DIVISIONS_] CACHE_ALIGN;
    /// @brief An array of pools (one pool per thread) of PWFCombStackRec structs.
    PWFCombStackRec ** volatile mem_state;
    volatile uint64_t ** flush;
    /// @brief Pointer to an array, where threads announce the requests (i.e. pushes and pops) that want to perform to the object.
    PWFCombRequestRec * volatile request;
    volatile uint64_t ** comb_round;
    /// @brief The number of threads that use this instance of PWFstack.
    uint32_t nthreads;
    /// @brief The maximum backoff value that could be used by this instance of PWFstack.
    int MAX_BACK;
} PWFCombStackStruct;

///  @brief This function initializes an instance of the PWFstack persistent stack implementation.
///
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any push or pop operation.
///  
/// @param stack A pointer to an instance of the PWFstack persistent stack implementation.
/// @param nthreads The number of threads that will use the PWFstack persistent stack implementation.
/// @param max_backoff The maximum value for backoff (usually this is lower than 100).
void PWFCombStackInit(PWFCombStackStruct *stack, uint32_t nthreads, int max_backoff);

/// @brief This function should be called once by each thread before it applies any operation to the PWFstack persistent stack implementation.
///
/// @param stack A pointer to an instance of the PWFstack persistent stack implementation.
/// @param th_state A pointer to thread's local state of PWFstack.
/// @param nthreads The number of threads that will use the PWFstack persistent stack implementation.
/// @param pid The pid of the calling thread.
void PWFCombStackThreadStateInit(PWFCombStackStruct *stack, PWFCombStackThreadState *th_state, uint32_t nthreads, int pid);

/// @brief This function adds (i.e. pushes) a new element to the top of the stack.
/// This element has a value equal with arg.
///
/// @param stack A pointer to an instance of the PWFstack persistent stack implementation.
/// @param th_state A pointer to thread's local state of PWFstack.
/// @param arg The push operation will insert a new element to the stack with value equal to arg.
/// @param pid The pid of the calling thread.
void PWFCombStackPush(PWFCombStackStruct *stack, PWFCombStackThreadState *th_state, ArgVal arg, int pid);

/// @brief This function removes (i.e. pops) an element from the top of the stack and returns its value.
///
/// @param stack A pointer to an instance of the PWFstack persistent stack implementation.
/// @param th_state A pointer to thread's local state of PWFstack.
/// @param pid The pid of the calling thread.
/// @return The value of the removed element. 
RetVal PWFCombStackPop(PWFCombStackStruct *stack, PWFCombStackThreadState *th_state, int pid);

#endif
