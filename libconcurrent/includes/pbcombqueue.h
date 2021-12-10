/// @file pbcombqueue.h
/// @authors Panagiota Fatourou and Nikolaos D. Kallimanis and Eleftherios Kosmas
/// @brief This file exposes the API of the PBqueue, which is a persistent queue implementation.
/// An example of use of this API is provided in benchmarks/pbcombqueuebench.c file.
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
#include <fastrand.h>
#include <pool.h>
#include <queue-stack.h>

/// @brief PBCombQueueStruct stores the state of an instance of the PBqueue persistent queue implementation.
/// PBCombQueueStruct should be initialized using the PBCombQueueStructInit function.
typedef struct PBCombQueueStruct {
    /// @brief A PBcomb instance for servicing the enqueue operations.
    PBCombStruct enqueue_struct CACHE_ALIGN;
    /// @brief A PBcomb instance for servicing the dequeue operations.
    PBCombStruct dequeue_struct CACHE_ALIGN;
    /// @brief A pointer to the last inserted element.
    volatile Node *last CACHE_ALIGN;
    /// @brief A pointer to the first inserted element.
    volatile Node *first CACHE_ALIGN;
    /// @brief A guard node that it is used only during the initialization of the queue.
    Node guard CACHE_ALIGN;
} PBCombQueueStruct;

/// @brief PBCombQueueThreadState stores each thread's local state for a single instance of PBqueue.
/// For each instance of PBqueue, a discrete instance of PBCombQueueThreadState should be used.
typedef struct PBCombQueueThreadState {
    /// @brief A PBCombThreadState struct for the instance of PBcomb that serves the enqueue operations.
    PBCombThreadState enqueue_thread_state;
    /// @brief A PBCombThreadState struct for the instance of PBcomb that serves the dequeue operations.
    PBCombThreadState dequeue_thread_state;
} PBCombQueueThreadState;

/// @brief This function initializes an instance of the PBqueue persistent queue implementation.
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any enqueue or dequeue operation.
///
/// @param queue_object_struct A pointer to an instance of the PBqueue persistent queue implementation.
/// @param nthreads The number of threads that will use the PBqueue persistent queue implementation.
void PBCombQueueInit(PBCombQueueStruct *queue_object_struct, uint32_t nthreads);

/// @brief This function should be called once before the thread applies any operation to the PBqueue persistent queue implementation.
///
/// @param object_struct A pointer to an instance of the PBqueue persistent queue implementation.
/// @param lobject_struct A pointer to thread's local state of PBqueue.
/// @param pid The pid of the calling thread.
void PBCombQueueThreadStateInit(PBCombQueueStruct *object_struct, PBCombQueueThreadState *lobject_struct, int pid);

/// @brief This function adds (i.e. enqueues) a new element to the back of the queue.
/// This element has a value equal with arg.
///
/// @param object_struct A pointer to an instance of the PBqueue persistent concurrent queue implementation.
/// @param lobject_struct A pointer to thread's local state of PBqueue persistent.
/// @param arg The enqueue operation will insert a new element to the queue with value equal to arg.
/// @param pid The pid of the calling thread. 
void PBCombQueueApplyEnqueue(PBCombQueueStruct *object_struct, PBCombQueueThreadState *lobject_struct, ArgVal arg, int pid);

/// @brief This function removes (i.e. dequeues) an element from the front of the queue and returns its value.
///
/// @param object_struct A pointer to an instance of the PBqueue persistent queue implementation.
/// @param lobject_struct A pointer to thread's local state of PBqueue.
/// @param pid The pid of the calling thread.
/// @return The value of the removed element.
RetVal PBCombQueueApplyDequeue(PBCombQueueStruct *object_struct, PBCombQueueThreadState *lobject_struct, int pid);

#endif
