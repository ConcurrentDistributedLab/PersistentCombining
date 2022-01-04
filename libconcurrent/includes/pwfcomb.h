/// @file pwfcomb.h
/// @authors Panagiota Fatourou and Nikolaos D. Kallimanis and Eleftherios Kosmas
/// @brief This file exposes the API of the PWFcomb, which is a persistent wait-free combining object.
/// An example of use of this API is provided in benchmarks/pwfcombbench.c file.
///
/// For a more detailed description see the original publication:
/// Panagiota Fatourou, Nikolaos D. Kallimanis, and Eleftherios Kosmas. "The Performance Power of Software Combining in Persistence".
/// ACM SIGPLAN Notices. Principles and Practice of Parallel Programming (PPoPP) 2022.
///  @copyright Copyright (c) 2021
#ifndef _SIM_PERSISTENT_H_
#define _SIM_PERSISTENT_H_

#include <stdint.h>
#include <config.h>
#include <primitives.h>
#include <tvec.h>
#include <fastrand.h>
#include <threadtools.h>
#include <fam.h>

#define _SIM_PERSISTENT_LOCAL_POOL_SIZE_ 2
#define _SIM_PERSISTENT_FAD_DIVISIONS_   2

#if _SIM_PERSISTENT_LOCAL_POOL_SIZE_ < 2
#    error PWFcomb persistent combining object is improperly configured
#endif

/// @brief This struct stores the data for a copy of the simulated object's state.
typedef struct PWFCombStateRec {                             
    /// @brief A pointer to the array of return values.
    RetVal *return_val;
    /// @brief A vector of toggles, one per running thread. This toggle indicates if the corresponding running thread has a peding request or not.
    ToggleVector deactivate;
    ToggleVector index;
    /// @brief The actual data of the simulated object's state.
    ObjectState st;
#ifdef DEBUG
    int counter;
    int rounds;
#endif
    uint64_t __flex[1];
} PWFCombStateRec;

/// @brief A macro for calculating the size of the PWFCombStateRec struct for a specific amount of threads.
#define PWFCombObjectStateSize(nthreads) (sizeof(PWFCombStateRec) + 2 * _TVEC_VECTOR_SIZE(nthreads) + (nthreads) * sizeof(RetVal))

/// @brief pointer_t should not used directely by user. This struct is used by PWFcomb for pointing to the 
/// most rescent and valid copy of the simulated object's state. It also contains a 40-bit sequence number
/// for avoiding the ABA problem.
typedef union pointer_t {
    struct StructData{
        int64_t seq : 40;
        int32_t index : 24;
    } struct_data;
    int64_t raw_data;
} pointer_t;

/// @brief PWFCombThreadState stores each thread's local state for a single instance of PWFcomb.
/// For each instance of PWFcomb, a discrete instance of PWFCombThreadState should be used.
typedef struct PWFCombThreadState {
    ToggleVector mask;
    ToggleVector index;
    ToggleVector diffs;
    ToggleVector l_activate;
    ToggleVector diffs_copy;
    /// @brief Current backoff value.
    int backoff;
} PWFCombThreadState;

typedef struct PWFCombRequestRec {
    ArgVal arg : 62;
    /// @brief A boolean field that determines if this request is valid or not.
    uint64_t valid : 2;
    uint64_t pad[15];
} PWFCombRequestRec;

typedef struct PWFCombPersistentState{
    /// @brief Index (i.e. pointer) to a PWFCombStateRec structs that contains the most recent and valid copy of simulated object's state.
    volatile pointer_t S;
} PWFCombPersistentState;

/// @brief PWFCombStruct stores the state of an instance of the a PWFcomb persistent combining object.
/// PWFCombStruct should be initialized using the PWFCombStructInit function.
typedef struct PWFCombStruct {
    volatile PWFCombPersistentState *pstate;
    /// @brief A vector of toggle bits, one toggle per NUMA node. The object could also work fine with a single such toggle.
    /// However, by using one toggle per NUMA node, the performance is increased substantially.
    ToggleVector activate[_SIM_PERSISTENT_FAD_DIVISIONS_] CACHE_ALIGN;
    /// @brief An array of pools (one pool per thread) of PWFCombStateRec structs.
    PWFCombStateRec ** volatile mem_state;
    volatile uint64_t ** flush;
    /// @brief Pointer to an array, where threads announce the requests that want to perform to the object.
    PWFCombRequestRec * volatile request;
    volatile uint64_t ** comb_round;
    /// @brief The number of threads that use this instance of PWFcomb.
    uint32_t nthreads;
    /// @brief The maximum backoff value that could be used by this instance of PWFcomb.
    int MAX_BACK;
} PWFCombStruct;

/// @brief This function initializes an instance of the PWFcomb persistent combining object.
///
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any request by using the PWFCombApplyOp function.
///
/// @param l A pointer to an instance of the PWFcomb object.
/// @param nthreads The number of threads that will use  this instance of the PWFcomb object.
/// @param max_backoff The maximum value for backoff (usually this is lower than 100).
void PWFCombInit(PWFCombStruct *l, uint32_t nthreads, int max_backoff);

/// @brief This function should be called once by each thread before it applies any operation to the PWFcomb combining object.
/// 
/// @param th_state A pointer to thread's local state of PWFcomb.
/// @param nthreads The number of threads that will use this instance of PWFcomb.
/// @param pid The pid of the calling thread.
void PWFCombThreadStateInit(PWFCombThreadState *th_state, uint32_t nthreads, int pid);

/// @brief This function is called whenever a thread wants to apply an operation to the simulated persistent combining object.
///
/// @param l A pointer to an instance of the PWFcomb persistent wait-free combining object.
/// @param th_state A pointer to thread's local state for a specific instance of PWFcomb.
/// @param sfunc A serial function that the PWFcomb instance should execute, while applying requests announced by active threads.
/// @param arg The argument of the request that the thread wants to apply.
/// @param pid The pid of the calling thread.
/// @return RetVal The return value of the deactivate request. 
Object PWFCombApplyOp(PWFCombStruct *l, PWFCombThreadState *th_state, RetVal (*sfunc)(void *, ArgVal, int), Object arg, int pid);

#endif
