/// @file pwfcombqueue.h
/// @authors Panagiota Fatourou and Nikolaos D. Kallimanis and Eleftherios Kosmas
/// @brief This file exposes the API of the PWFqueue, which is a persistent wait-free queue object.
/// An example of use of this API is provided in benchmarks/pwfcombqueuekbench.c file.
///
/// For a more detailed description see the original publication:
/// Panagiota Fatourou, Nikolaos D. Kallimanis, and Eleftherios Kosmas. "The Performance Power of Software Combining in Persistence".
/// ACM SIGPLAN Notices. Principles and Practice of Parallel Programming (PPoPP) 2022.
///  @copyright Copyright (c) 2021
#ifndef _PWFCOMBQUEUE_PERSISTENT_H_
#define _PWFCOMBQUEUE_PERSISTENT_H_

#include <config.h>
#include <primitives.h>
#include <tvec.h>
#include <fastrand.h>
#include <pool.h>
#include <threadtools.h>
#include <pwfcomb.h>
#include <queue-stack.h>

/// @brief This struct stores the state of the queue that handles the Enqueue operatios.
typedef struct PWFCombQueueEnqRec {
    /// @brief A vector of toggles, one per running thread. This toggle indicates if the corresponding running thread has a peding request or not.
    ToggleVector deactivate;
    ToggleVector index;
    Node *first;
    Node *last;
    /// @brief A pointer to the tail of the queue, i.e. at the node that was inserted last.
    Node *tail;
#ifdef DEBUG
    int64_t counter;
#endif
    uint64_t __flex[1];
} PWFCombQueueEnqRec;

/// @brief A macro for calculating the size of the PWFCombQueueEnqRec struct for a specific amount of threads.
#define PWFCombQueueEnqStateSize(N) (sizeof(PWFCombQueueEnqRec) + 2 * _TVEC_VECTOR_SIZE(N))

/// @brief This struct stores the state of the queue that handles the Dequeue operatios.
typedef struct PWFCombQueueDeqState {
    /// @brief A vector of toggles, one per running thread. This toggle indicates if the corresponding running thread has a peding request or not.
    ToggleVector deactivate;
    ToggleVector index;
    /// @brief A pointer to the array of return values.
    RetVal *return_val;
    /// @brief A pointer to the head of the queue, i.e. at the node that was inserted first.
    Node *head;
#ifdef DEBUG
    int64_t counter;
#endif
    uint64_t __flex[1];
} PWFCombQueueDeqState;

/// @brief A macro for calculating the size of the PWFCombQueueState struct for a specific amount of threads.
#define PWFCombQueueDeqStateSize(N) (sizeof(PWFCombQueueDeqState) + 2 * _TVEC_VECTOR_SIZE(N) + (N) * sizeof(RetVal))

/// @brief PWFCombQueueThreadState stores each thread's local state for a single instance of PWFqueue.
/// For each instance of PWFqueue, a discrete instance of PWFCombQueueThreadState should be used.
typedef struct PWFCombQueueThreadState {
    ToggleVector mask;
    ToggleVector deq_index;
    ToggleVector enq_index;
    ToggleVector diffs;
    ToggleVector diffs_copy;
    ToggleVector l_activate;
    ToggleVector tmp_toggles;
    SynchPoolStruct pool_node;               
    int deq_local_index;
    int enq_local_index;
    /// @brief Current backoff value.
    int backoff;
} PWFCombQueueThreadState;

/// @brief PWFCombQueueStruct stores the state of an instance of the PWFqueue.
/// PWFCombQueueStruct should be initialized using the PWFCombQueueStructInit function.
typedef struct PWFCombQueueStruct {
    volatile pointer_t ES CACHE_ALIGN;
    volatile pointer_t DS CACHE_ALIGN;
    // Guard node
    // Do not set this as const node
    Node guard CACHE_ALIGN;
    // Pointers to shared data
    ToggleVector activate_enq[_SIM_PERSISTENT_FAD_DIVISIONS_] CACHE_ALIGN;
    ToggleVector activate_deq[_SIM_PERSISTENT_FAD_DIVISIONS_];
    PWFCombQueueEnqRec ** volatile EState;
    PWFCombQueueDeqState ** volatile DState;
    volatile uint64_t ** Eflush;
    volatile uint64_t ** Dflush;
    PWFCombRequestRec * volatile ERequest;
    PWFCombRequestRec * volatile DRequest;
    volatile uint64_t ** Ecomb_round;
    volatile uint64_t ** Dcomb_round;
    uint32_t nthreads;
    int MAX_BACK;
} PWFCombQueueStruct;

/// @brief This function initializes an instance of the PWFqueue persistent queue implementation.
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any enqueue or dequeue operation.
///
/// @param queue A pointer to an instance of the PWFqueue persistent queue implementation.
/// @param nthreads The number of threads that will use the PWFqueue persistent queue implementation.
/// @param max_backoff The maximum value for backoff (usually this is much lower than 100).
void PWFCombQueueInit(PWFCombQueueStruct *queue, uint32_t nthreads, int max_backoff);

/// @brief This function should be called once before the thread applies any operation to the PWFqueue concurrent queue implementation.
///
/// @param queue A pointer to an instance of the PWFqueue persistent queue implementation.
/// @param th_state A pointer to thread's local state of PWFqueue.
/// @param pid The pid of the calling thread.
void PWFCombQueueThreadStateInit(PWFCombQueueStruct *queue, PWFCombQueueThreadState *th_state, int pid);

/// @brief This function adds (i.e. enqueues) a new element to the back of the queue.
/// This element has a value equal with arg.
///
/// @param queue A pointer to an instance of the PWFqueue persistent queue implementation.
/// @param th_state A pointer to thread's local state of PWFqueue.
/// @param arg The enqueue operation will insert a new element to the queue with value equal to arg.
/// @param pid The pid of the calling thread.
void PWFCombQueueEnqueue(PWFCombQueueStruct *queue, PWFCombQueueThreadState *th_state, ArgVal arg, int pid);

/// @brief This function removes (i.e. dequeues) an element from the front of the queue and returns its value.
///
/// @param queue A pointer to an instance of the PWFqueue persistent queue implementation.
/// @param th_state A pointer to thread's local state of PWFqueue.
/// @param pid The pid of the calling thread.
/// @return The value of the removed element.
RetVal PWFCombQueueDequeue(PWFCombQueueStruct *queue, PWFCombQueueThreadState *th_state, int pid);

#endif
