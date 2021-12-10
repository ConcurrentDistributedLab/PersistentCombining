/// @file config.h
/// @author Nikolaos D. Kallimanis (nkallima@gmail.com)
/// @brief This file provides important parameters and constants for the benchmarks and the library.
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>

/// @brief Defines the default value of maximum local work that each thread executes between two consecutive of the
/// benchmarked operation. A zero value means there is no work between two consecutive calls.
/// Large values usually reduce system's contention, i.e. threads perform operations less frequentely.
/// In constrast, small values (but not zero) increase system's contention. Please avoid to set this value
/// equal to zero, since some algorithms may produce unreallistically high performance (i.e. long runs
/// and unrealistic low numbers of cache misses).
/// Default value is 64.
#ifndef SYNCH_MAX_WORK
#    define SYNCH_MAX_WORK         64
#endif

/// @brief Defines the default total number of the executed operations. Notice that benchmarks for stacks and queues
/// execute SYNCH_RUNS pairs of operations (i.e. pairs of push/pops or pairs of enqueues/dequeues).
/// Default value is 1000000.
#define SYNCH_RUNS                 1000000

/// @brief Define DEBUG, in case you want to debug some parts of the code or to get some  useful performance statistics.
/// Note that the validation.sh script enables this definition by default. In some cases, this may introduces
/// some performance loses. Thus, in case you want to perform benchmarking keeps this undefined.
/// By default, this flag is disabled.
//#define DEBUG

/// @brief This definition disbales backoff in all algorithms that are using it for reducing system's contention.
/// By default, this flag is disabled.
//#define SYNCH_DISABLE_BACKOFF

#define Object                     int64_t

/// @brief Defines the type of the return value that most operations in the provided benchmarks return.
/// For instance, the return values of all pop and dequeue operations (implemented by the provided stacks and queue
/// objects) return a value of type RetVal. It is assumed that the target architecture is able to atomically read/write
/// this type If this type is of 32 or 64 bits could be atomically read or written by most machine architectures.
/// However, a value of 128 bits or more may not be supported (in most cases x86_64 supports types of 128 bits).
/// By default, this definition is equal to int64_t
#define RetVal                     int64_t

/// @brief Defines the type of the argument value of atomic operations provided by the implemented concurrent objects.
/// For example, all push and enqueue operations (implemented by the provided stacks and queue objects) get an
/// argument of type ArgVal. It is assumed that the target architecture is able to atomically read/write this type.
/// If this type is of 32 or 64 bits could be atomically read or written by most machine architectures.
/// However, a value of 128 bits or more may not be supported (in most cases x86_64 supports types of 128 bits).
/// By default, this definition is equal to int64_t
#define ArgVal                     int64_t

/// @brief Whenever the `SYNCH_NUMA_SUPPORT` option is enabled, the runtime will detect the system's number of NUMA nodes
/// and will setup the environment appropriately. However, significant performance benefits have been observed by
/// manually setting-up the number of NUMA nodes manually (see the `--numa_nodes` option). For example, the performance
/// of the H-Synch family algorithms on an AMD EPYC machine consisting of 2x EPYC 7501 processors (i.e., 128 hardware threads)
/// is much better by setting `--numa_nodes` equal to `2`. Note that the runtime successfully reports that the available
/// NUMA nodes are `8`, but this value is not optimal for H-Synch in this configuration. An experimental analysis for
/// different values of `--numa_nodes` may be needed.
#define SYNCH_NUMA_SUPPORT

/// @brief This definition enables some optimizations on memory allocation that seems to greately improve the performance
/// on AMD Epyc multiprocessors. This flag seems to double the performance in CC-Synch and H-Synch algorithms.
/// In contrast to AMD processors, this option introduces serious performance overheads in Intel Xeon processors. 
/// Thus, a careful experimental analysis is needed in order to show the possible benefits of this option.
/// By default, this flag is enabled.
#define SYNCH_COMPACT_ALLOCATION

/// @brief This definition disables node recycling in the concurrent stack and queue implementations that support memory
/// reclamation. This may have negative impact on performance, in some cases. More on this on the README.md file.
/// By default, this flag is disabled.
//#define SYNCH_POOL_NODE_RECYCLING_DISABLE

/// @brief By enabling this definition, the Performance Application Programming Interface (PAPI library) is used for
/// getting performance counters during the execution of benchmarks. In this case, the PAPI library (i.e. libpapi)
/// should be install and appropriately configured.
/// By default, this flag is disabled.
//#define SYNCH_TRACK_CPU_COUNTERS

/// @brief By enabling this flag, any concurrent stack implementation that supports elimination for improving performance
/// avoids to use it. This should be used only for identifying performance bottlenecks, since it may introduce performance overhead.
/// By default, this flag is disabled.
//#define SYNCH_DISABLE_ELIMINATION_ON_STACKS

/// @brief By enabling this definition, we enable NVDIMM (non-volatile DIMMs) support for the provided persistent algorithms.
/// In case that you do not want to use NVDIMM-support, this definition should be commented out. The `SYNCH_PERSISTENT_DEV_PATH`
/// defines a default path where the NVDIMM device is mounted. This should be modified according user's needs. It is worth pointing
/// that in case that the framework is build with the `SYNCH_ENABLE_PERSISTENT_MEM` enabled and the system is not equipped with 
/// a NVDIMM device, the code will follow a fallback path and it will use the directory defined in the `SYNCH_PERSISTENT_DEV_PATH_FALLBACK`.
#define SYNCH_ENABLE_PERSISTENT_MEM

/// @brief This definition indicates a default location where the NVDIMM device is mounted. In case that the system is not equipped with a 
/// NVDIMM device, the code will follow a fallback path and it will use the directory defined in the `SYNCH_PERSISTENT_DEV_PATH_FALLBACK`.
#define SYNCH_PERSISTENT_DEV_PATH          "/mnt/pmem_fsdax0/"

/// @brief This definition indicates a fallback path for creating memory-maped file used for allocations of the persisted algorithms.
/// An option is to use a shared memory directory (tmpfs filesystem), but in this case the stored data should be volatile. 
/// Another option is to use a path on some storage device (e.g. NVMe, etc.), but in this case the performance should be lower 
/// compared to a NVDIMM device.
#define SYNCH_PERSISTENT_DEV_PATH_FALLBACK "/dev/shm/"

/// @brief This constant defines the initial amount of persisted memory that each thread should allocate for using persistent data-structures.
#define SYNCH_PERSISTENT_MEM_SIZE_INIT     (128 * 1024 * 1024)   // 128MB of persistent memory

/// @brief This flags converts any PWD operation (i.e. pmem_flush) to a dummy operation. This should be used only for identifying 
/// performance bottlenecks on the persistent data-structures. This flag has any impact on the non-peristent data-structures.
/// By default, this flag is disabled.
//#define SYNCH_DISABLE_PWBS

/// @brief This flags converts any PSYNC operation (i.e. pmem_drain) to a dummy operation. This should be used only for identifying 
/// performance bottlenecks on the persistent data-structures. This flag has any impact on the non-peristent data-structures.
/// By default, this flag is disabled.
//#define SYNCH_DISABLE_PSYNCS

/// @brief This flag indicates to the Synch framework to keep statistics on the amount of PWB operations. This flag may add 
/// some overhead to persistent data-structures. By default, this flag is enabled.
//#define SYNCH_COUNT_PWBS


//#define _EMULATE_FAA_
//#define _EMULATE_SWAP_
#endif
