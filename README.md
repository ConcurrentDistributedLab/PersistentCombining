# Summary

The availability of Non-Volatile Memory DRAM (known as NVMM) enables the design of recoverable concurrent algorithms. In our PPoPP 2022 paper entitled "The Performance Power of Software Combining in Persistence", we study the power of software combining in achieving recoverable synchronization and designing persistent data structures. We identify three persistence principles, crucial for performance, that an algorithm’s designer has to take into consideration when designing highly-efficient recoverable synchronization protocols or data structures. We illustrate how to make the appropriate design decisions in all stages of devising recoverable combining protocols to respect these principles.

The paper presents two recoverable software combining protocols, satisfying different progress properties, that are many times faster and have much lower persistence cost than a large collection of existing persistent techniques for achieving scalable synchronization. We build fundamental recoverable data structures, such as stacks and queues, based on these protocols that outperform by far existing recoverable implementations of such data structures. It also provides the first recoverable implementation of a concurrent heap and present experiments to show that it has good performance when the size of the heap is not very large. 

Here we provide the code of these algorithms, together with the necessary documentation for reproducing the experimental results presented in the paper, regarding our algorithms. To implement our algorithms, we make use of the [Sync Framework](https://github.com/nkallima/sim-universal-construction).

# Updates

An up-to-date version of the code provided here, together with additional recoverable implementations, can be found in our working [GitHub repository](https://github.com/ConcurrentDistributedLab/PersistentCombining).

# Algorithms
The following table presents a summary of the persistent concurrent data-structures.
| Persistent Object     |                Provided Implementations                           |
| --------------------- | ----------------------------------------------------------------- |
| Combining Objects     | PBcomb [1,2,3]                                                    |
|                       | PWFcomb [1,2,3]                                                   |
| Persistent Queues     | PBqueue [1,2,3]                                                   |
|                       | PWFqueue [1,2,3]                                                  |
| Persistent Stacks     | PBstack [1,2,3]                                                   |
|                       | PWFstack [1,2,3]                                                  |
| Persistent Heaps      | PBheap [1,2]                                                      |

# Experiments

We execute each experiment 10 times (runs) and display averages. Each run simulates 10^7 atomic operations in total, with each of the n threads simulating 10^7/n operations. In the experiments for the stacks (queues), each thread performs pairs of Push and Pop (Enqueue and Dequeue) starting from an empty data structure. 

# Reproduce experimental results

First, run the `figures_pcomb_compile.sh` script to compile the executables. Then, run the `figures_pcomb_run.sh` script to produce the results of each figure in [1], regarding our algorithms. The script creates the output files in the `results` directory. 

Each figure presents either the average throughput (million operations per second) or the average number of pwbs per operation, as the number of threads increases. The following table presents more details for each figure.

| Figures  | Experiment Description                                                            |
| -------- | --------------------------------------------------------------------------------- |
| 1a       | throughput for PBcomb and PWFcomb                                                 |
| 1b       | number of pwbs for PBcomb and PWFcomb                                             |
| 1c       | similar with 1a & versions of PBcomb and PWFcomb that do not execute any psync    |
| 2a       | throughput for PBqueue (with and without recycling) and for PWFqueue              |
| 2b       | number of pwbs for PBqueue and PWFqueue                                           |
| 2c       | throughput for PBqueue and PWFqueue that do not execute any pwb                   |
| 3a       | throughput for PBstack and PWFstack (with and without recycling or elimination)   |
| 3b       | throughput for PBheap with heap size 64, 128, 256, 512, and 1024                  |

# Memory reclamation (stacks and queues)

We incorporate a pool mechanism (see `includes/pool.h`) that efficiently allocates and de-allocates memory for the provided concurrent stack and queue implementations. By default, memory-reclamation is enabled. To disable it, the `SYNCH_POOL_NODE_RECYCLING_DISABLE` option should be enabled in `config.h`.

The following table shows the memory reclamation characteristics of the provided persistent objects.

| Persistent Object     | Provided Implementations | Memory Reclamation |
| --------------------- | ------------------------ | ------------------ |
| Persistent Queues     | PBqueue [1,2,3]          | Supported          |
|                       | PWFqueue [1,2,3]         | Not Supported      |
| Persistent Stacks     | PBstack [1,2,3]          | Supported          |
|                       | PWFstack [1,2,3]         | Supported          |
| Persistent Heaps      | PBheap [1,2]             | Supported          |

# Requirements

- A modern 64-bit machine.
- A recent Linux distribution.
- As a compiler, gcc of version 4.8 or greater is recommended, but you may also try to use icx or clang.
- Building requires the development versions of the following packages:
    - `libatomic`
    - `libnuma`
    - `libpapi` in case that the `SYNCH_TRACK_CPU_COUNTERS` flag is enabled in `libconcurrent/config.h`.
    - `libvmem`, necessary for building the collection of persistent objects.
    - `libpmem`, necessary for building the collection of persistent objects.

    Depending on where these packages are installed, the appropriate environment variable (e.g., the `LD_LIBRARY_PATH` variable for Linux) should contain the path to them.

# License

This code is provided under the [LGPL-2.1 License](https://github.com/ConcurrentDistributedLab/PersistentCombining/blob/main/LICENSE).

# References

[1] - Panagiota Fatourou, Nikolaos D. Kallimanis, and Eleftherios Kosmas. "The Performance Power of Software Combining in Persistence". ACM SIGPLAN Notices. Principles and Practice of Parallel Programming (PPoPP) 2022.

[2] - Panagiota Fatourou, Nikolaos D. Kallimanis, Eleftherios Kosmas. "Brief Announcement: Persistent Software Combining". In 35th International Symposium on Distributed Computing (DISC 2021). Schloss Dagstuhl-Leibniz-Zentrum für Informatik, 2021.

[3] - Panagiota Fatourou, Nikolaos D. Kallimanis, Eleftherios Kosmas. "Persistent Software Combining." arXiv preprint arXiv:2107.03492, 2021.

# Funding

Panagiota Fatourou: Supported by the EU Horizon 2020, Marie Sklodowska-Curie project with GA No 101031688.

Eleftherios Kosmas: Co-financed by Greece and the European Union (European Social Fund- ESF) through the Operational Programme «Human Resources Development, Education and Lifelong Learning» in the context of the project “Reinforcement of Postdoctoral Researchers - 2nd Cycle” (MIS-5033021), implemented by the State Scholarships Foundation (IKY).

# Contact

For any further information, please contact any of the authors: faturu (at) ics.forth.gr, nkallima (at) ics.forth.gr, ekosmas (at) csd.uoc.gr.
