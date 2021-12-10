#!/bin/bash

make clean  &> out
# ==================================================================================================
# Figure 1.a (AtomicFloat - throughput)
# -------------------------------------
printf "\nCompiling PB and PWF algorithms for AtomicFloat-throughput (Figure 1a).\n"
make  &> out
mv build/bin/pbcombbench.run build/bin/pbcomb.run 
mv build/bin/pwfcombbench.run build/bin/pwfcomb.run 
printf "AtomicFloat-throughput (Figure 1a) - Compiling completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.b (AtomicFloat - number of pwbs)
# -----------------------------------------
printf "\nCompiling PB and PWF algorithms for AtomicFloat-number-of-pwbs (Figure 1b).\n"
make x86_64_pwbs  &> out
mv build/bin/pbcombbench.run build/bin/pbcomb_pwbs.run 
mv build/bin/pwfcombbench.run build/bin/pwfcomb_pwbs.run 
mv build/bin/pbcombqueuebench.run build/bin/pbqueue_pwbs.run 
mv build/bin/pwfcombqueuebench.run build/bin/pwfqueue_pwbs.run 
printf "AtomicFloat-number-of-pwbs (Figure 1b) - Compiling completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.c (AtomicFloat - no psync)
# -----------------------------------
printf "\nCompiling PB and PWF algorithms for AtomicFloat-(Psync=off) (Figure 1c).\n"
# Compile this paper's code
make x86_64_no_psync  &> out
mv build/bin/pbcombbench.run build/bin/pbcomb_no_psync.run 
mv build/bin/pwfcombbench.run build/bin/pwfcomb_no_psync.run 
printf "AtomicFloat-(Psync=off) (Figure 1c) - Compiling completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.d (Queue - throughput)
# -------------------------------
printf "\nCompiling PB and PWF algorithms for Queue-throughput (Figure 1d).\n"
make  &> out
mv build/bin/pbcombqueuebench.run build/bin/pbqueue.run 
mv build/bin/pwfcombqueuebench.run build/bin/pwfqueue.run 
make x86_64_no_recycle &> out
mv build/bin/pbcombqueuebench.run build/bin/pbqueue_no_rec.run 
printf "Queue-throughput (Figure 1d) - Compiling completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.e (Queue - number of pwbs)
# -----------------------------------
printf "\nCompiling PB and PWF algorithms for Queue-number-of-pwbs (Figure 1e).\n"
make x86_64_pwbs  &> out
mv build/bin/pbcombqueuebench.run build/bin/pbqueue_pwbs.run 
mv build/bin/pwfcombqueuebench.run build/bin/pwfqueue_pwbs.run 
printf "Queue-number-of-pwbs (Figure 1e) - Compiling completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.f (Queue - throughput with no pwbs)
# --------------------------------------------
printf "\nCompiling PB and PWF algorithms for Queue-throughput-with-no-pwbs (Figure 1f).\n"
make x86_64_no_pwb  &> out
mv build/bin/pbcombqueuebench.run build/bin/pbqueue_no_pwbs.run 
mv build/bin/pwfcombqueuebench.run build/bin/pwfqueue_no_pwbs.run 
make x86_64_no_pwb  &> out
printf "Queue-throughput-with-no-pwbs (Figure 1f) - Compiling completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.g (Stack - throughput)
# -------------------------------
printf "\nCompiling PB and PWF algorithms for Stack-throughput (Figure 1g).\n"
make  &> out
mv build/bin/pbcombstackbench.run build/bin/pbstack.run 
mv build/bin/pwfcombstackbench.run build/bin/pwfstack.run 
make x86_64_no_recycle  &> out
mv build/bin/pbcombstackbench.run build/bin/pbstack_no_rec.run 
mv build/bin/pwfcombstackbench.run build/bin/pwfstack_no_rec.run 
make x86_64_no_elimination  &> out
mv build/bin/pbcombstackbench.run build/bin/pbstack_no_elim.run 
mv build/bin/pwfcombstackbench.run build/bin/pwfstack_no_elim.run 
printf "Stack-throughput (Figure 1g) - Compiling completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.h (Heap - throughput)
# ------------------------------
printf "\nCompiling PB and PWF algorithms for Heap-throughput (Figure 1f).\n"
# PBheap-1024
make x86_64_heap_1024  &> out
mv build/bin/pbcombheapbench.run build/bin/pbheap1024.run 
# PBheap-512
make x86_64_heap_512  &> out
mv build/bin/pbcombheapbench.run build/bin/pbheap512.run 
# PBheap-256
make x86_64_heap_256  &> out
mv build/bin/pbcombheapbench.run build/bin/pbheap256.run 
# PBheap-128
make x86_64_heap_128  &> out
mv build/bin/pbcombheapbench.run build/bin/pbheap128.run 
# PBheap-64
make x86_64_heap_64  &> out
mv build/bin/pbcombheapbench.run build/bin/pbheap64.run 
printf "Heap-throughput (Figure 1h) - Compiling completed.\n"
# ==================================================================================================