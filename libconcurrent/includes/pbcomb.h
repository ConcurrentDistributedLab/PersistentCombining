/// @file pbcomb.h
/// @authors Panagiota Fatourou and Nikolaos D. Kallimanis and Eleftherios Kosmas
/// @brief This file exposes the API of the PBcomb, which is a persistent combining object.
/// An example of use of this API is provided in benchmarks/pbcombbench.c file.
///
/// For a more detailed description see the original publication:
/// Panagiota Fatourou, Nikolaos D. Kallimanis, and Eleftherios Kosmas. "The Performance Power of Software Combining in Persistence".
/// ACM SIGPLAN Notices. Principles and Practice of Parallel Programming (PPoPP) 2022.
///  @copyright Copyright (c) 2021
#ifndef _PBCOMB_H_
#define _PBCOMB_H_

#include "config.h"
#include "primitives.h"

/// @brief The size of a pool of states that each running thread maintains.
#define PBCOMB_POOL_SIZE  2

/// @brief This struct describes a request (i.e.) to be applied to the PBcomb object.
typedef struct PBCombRequest {
    /// @brief The arguments of the operation.
    volatile ArgVal arg;                        //    Argument Args
    /// @brief Operation type.
    volatile uint64_t operation;                //    Function func
    /// @brief A boolean field that determines if this request is applied or not.
    volatile uint32_t activate;
    /// @brief A boolean field that determines if this request is valid or not.
    volatile uint32_t valid;
    /// @brief Padding space.
    uint64_t pad[1];
} PBCombRequest;

/// @brief This struct describes the state of the simulated object.
typedef struct PBCombStateRec {
    /// @brief The actual data of the state, which a pointer that points to the flex field.
    volatile void *state;
    /// @brief The return values, one per thread.
    volatile RetVal *return_value;
    /// @brief An array of booleans (one per running thread) that determines if the thread's corresponding request is applied or not.
    bool *deactivate;
    /// @brief A dummy field, the data of state, the array of return values and the array of `deactivate` booleans follow.
    uint64_t flex[0];
} PBCombStateRec;

typedef struct PBCombPersistentState{
    volatile PBCombStateRec *last_state;
} PBCombPersistentState;

/// @brief PBCombStruct stores the state of an instance of the a PBcomb persistent combining object.
/// PBCombStruct should be initialized using the PBCombStructInit function.
typedef struct PBCombStruct {
    volatile PBCombRequest *request;
    /// @brief A pointer to a function that may execute persistent operations 
    /// just before the releasing of the lock by the combiner (i.e. releasing the `lock` of `PBCombStruct`).
    void (*final_persist_func)(void *);
    /// @param after_persist_func A pointer to a function that may execute persistent operations
    /// just after the releasing of the lock by the combiner (i.e. releasing the `lock` of `PBCombStruct`).
    void (*after_persist_func)(void *);
    /// @brief This is an array of size `n`, where `n` is the number of runnning threads.
    /// The first entry of corresponds to thread with id 0, while the second entry corresponds to thread with 1, etc.
    /// Each entry contains the id of the numa node that the corresponding thread runs on.
    volatile uint32_t *numa_ids;
    /// @brief The number of threads that will use the PBcomb object.
    volatile uint32_t nthreads;
    /// @brief The number of NUMA nodes that the system is equipped of.
    volatile uint32_t numa_nodes;
    /// @brief The number of running threads per NUMA node.
    volatile uint32_t threads_per_node;
    /// @brief The size (in bytes) of simulated object's state.
    volatile uint32_t state_size;
    /// @brief This is an integer lock that allows a single combiner to serve requests at each point in time.
    volatile uint32_t lock CACHE_ALIGN;
    volatile uint64_t lock_value CACHE_ALIGN;
    /// @brief A pointer to the latest valid, persisted state of the simulated object.
    /// For performance reasons, we use a pool of PBCOMB_POOL_SIZE * `n` such states instead of 2 in the paper
    /// (`n` is the number of threads).
    volatile PBCombPersistentState *pstate;
    volatile void *aux;
#ifdef DEBUG
    volatile int32_t counter CACHE_ALIGN;
    volatile int32_t rounds;
#endif
} PBCombStruct;

/// @brief PBCombThreadState stores each thread's local state for a single instance of PBcomb.
/// For each instance of PBcomb, a discrete instance of PBCombThreadState should be used.
typedef struct PBCombThreadState {
    /// @brief The NUMA node of the current thread.
    uint32_t numa_node;
    /// @brief The id of the NUMA node of the current thread.
    uint32_t numa_id;
    /// @brief An index to the latest copy of the object's state used by this thread (as a combiner).
    uint32_t pool_index;                           // Bit MIndex
    /// @brief A pool PBCOMB_POOL_SIZE states per thread. This pool is used whenever this thread acts as a combiner.
    /// For performance reasons, we use a pool of PBCOMB_POOL_SIZE copies of the state per thread.
    PBCombStateRec *pool[PBCOMB_POOL_SIZE];        // StateRec MemState
} PBCombThreadState;

/// @brief This function initializes an instance of the PBcomb persistent combining object.
///
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any push or pop operation.
///
/// @param l A pointer to an instance of the PBcomb persistent combining object.
/// @param nthreads The number of threads that will use the PBcomb persistent combining object.
/// @param initial_state A pointer to object's initial state.
/// @param state_size The size (in bytes) of the initial state of the object.
void PBCombStructInit(PBCombStruct *l, uint32_t nthreads, void *initial_state, uint32_t state_size);

/// @brief This function is used by the combiners just after the application of all the pending operations
/// and just before releasing object's lock. The use of this function is to persist object's data that are not 
/// persisted by the serial function of PBCombApplyOp and these data are not contained in the `state` array.
/// Examples of use is the stack and queue implementations based on PBcomb, i.e. PBstack and PBqueue.
///
/// @param l A pointer to an instance of the PBcomb persistent combining object.
/// @param final_persist_func A pointer to a function that may execute persistent operations
/// just before the releasing of the lock by the combiner (i.e. releasing the `lock` of `PBCombStruct`).
void PBCombSetFinalPersist(PBCombStruct *l, void (*final_persist_func)(void *));

/// @brief This function is used by the combiners just after the application of all the pending operations
/// and just after releasing object's lock. The use of this function is to persist object's data that are not 
/// persisted by the serial function of PBCombApplyOp and these data are not contained in the `state` array.
/// Examples of use is the stack and queue implementations based on PBcomb, i.e. PBstack and PBqueue.
///
/// @param l A pointer to an instance of the PBcomb persistent combining object.
/// @param after_persist_func A pointer to a function that may execute persistent operations
/// just after the releasing of the lock by the combiner (i.e. releasing the `lock` of `PBCombStruct`).
void PBCombSetAfterPersist(PBCombStruct *l, void (*after_persist_func)(void *));

/// @brief This function should be called once before the thread applies any operation to the PBcomb object.
///
/// @param l A pointer to an instance of the PBcomb persistent combining object.
/// @param st_thread A pointer to thread's local state of PBcomb.
/// @param pid The pid of the calling thread.
void PBCombThreadStateInit(PBCombStruct *l, PBCombThreadState *st_thread, int pid);

/// @brief This function is called whenever a thread wants to apply an operation to the simulated concurrent object.
///
/// @param l A pointer to an instance of the PBcomb persistent combining object.
/// @param st_thread A pointer to thread's local state for a specific instance of PBcomb object.
/// @param sfunc A serial function that the PBcomb instance should execute, while applying requests announced by active threads.
/// @param arg The argument of the request that the thread wants to apply.
/// @param pid The pid of the calling thread.
/// @return RetVal The return value of the applied request.
RetVal PBCombApplyOp(PBCombStruct *l, PBCombThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), ArgVal arg, int pid);

#endif
