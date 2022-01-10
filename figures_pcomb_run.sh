#!/bin/bash

# Set the number of maximum threads
THREADS=96

# Set the step (starting from 0 and up to maximum threads, i.e. THREADS)
# for the number of threads to be used while running the experiments.
# A single thread execution will also be included.
STEP=12

# Create results folder
mkdir -p results
rm -rf results/*
mkdir -p results/tmp

# ==================================================================================================
# Figure 1.a (AtomicFloat - throughput)
# -------------------------------------
printf "\nCalculating the results of PB and PWF algorithms for AtomicFloat-throughput (Figure 1a).\n"
# Run experiments
# PBcomb
./bench.sh  pbcomb.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbcomb.tmp
# PWFcomb
./bench.sh  pwfcomb.run -t ${THREADS} -s ${STEP} -w 512 -b 11 | tee -a results/tmp/pwfcomb.tmp
# Collect results
echo -e "threads\t PBcomb" > results/tmp/pbcomb_res.tmp
cat results/tmp/pbcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%s\t%7.2f\n", $1, $9}' >> results/tmp/pbcomb_res.tmp
echo -e "PWFcomb" > results/tmp/pwfcomb_res.tmp
cat results/tmp/pwfcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pwfcomb_res.tmp
results_files_array=()
results_files_array+=("results/tmp/pbcomb_res.tmp")
results_files_array+=("results/tmp/pwfcomb_res.tmp")
paste -d"\t" ${results_files_array[@]} > results/figure1a.txt
rm -rf results/tmp/*
printf "AtomicFloat-throughput (Figure 1a) - Experiment completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.b (AtomicFloat - number of pwbs)
# -----------------------------------------
printf "\nCalculating the results of PB and PWF algorithms for AtomicFloat-number-of-pwbs (Figure 1b).\n"
# Run experiments
# PBcomb
./bench.sh  pbcomb_pwbs.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbcomb.tmp
# PWFcomb
./bench.sh  pwfcomb_pwbs.run -t ${THREADS} -s ${STEP} -w 512 -b 11 | tee -a results/tmp/pwfcomb.tmp
# Collect results
echo -e "threads\t PBcomb" > results/tmp/pbcomb_res.tmp
cat results/tmp/pbcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%s\n", $1}' >> results/tmp/pbcomb_res1.tmp
cat results/tmp/pbcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep num_pwbs | awk '{printf "%7.2f\n", $4}' >> results/tmp/pbcomb_res2.tmp
results_files_array=()
results_files_array+=("results/tmp/pbcomb_res1.tmp")
results_files_array+=("results/tmp/pbcomb_res2.tmp")
paste -d"\t" ${results_files_array[@]} >> results/tmp/pbcomb_res.tmp
echo -e "PWFcomb" > results/tmp/pwfcomb_res.tmp
cat results/tmp/pwfcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep num_pwbs | awk '{printf "%7.2f\n", $4}' >> results/tmp/pwfcomb_res.tmp
results_files_array=()
results_files_array+=("results/tmp/pbcomb_res.tmp")
results_files_array+=("results/tmp/pwfcomb_res.tmp")
paste -d"\t" ${results_files_array[@]} > results/figure1b.txt
rm -rf results/tmp/*
printf "AtomicFloat-number-of-pwbs (Figure 1b) - Experiment completed.\n"
# ==================================================================================================


# ==================================================================================================
# Figure 1.c (AtomicFloat - no psync)
# -----------------------------------
printf "\nCalculating the results of PB and PWF algorithms for AtomicFloat-(Psync=off) (Figure 1c).\n"
# Run experiments
# PBcomb-(Psync=off)
./bench.sh  pbcomb_no_psync.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbcomb.tmp
# PWFcomb-(Psync=off)
./bench.sh  pwfcomb_no_psync.run -t ${THREADS} -s ${STEP} -w 512 -b 11 | tee -a results/tmp/pwfcomb.tmp
# Collect results
echo -e "PBcomb-(Psync=off)" > results/tmp/pbcomb_res.tmp
cat results/tmp/pbcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pbcomb_res.tmp
echo -e "PWFcomb-(Psync=off)" > results/tmp/pwfcomb_res.tmp
cat results/tmp/pwfcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pwfcomb_res.tmp
results_files_array=()
results_files_array+=("results/figure1a.txt")
results_files_array+=("results/tmp/pbcomb_res.tmp")
results_files_array+=("results/tmp/pwfcomb_res.tmp")
paste -d"\t" ${results_files_array[@]} > results/figure1c.txt
rm -rf results/tmp/*
printf "AtomicFloat-(Psync=off) (Figure 1c) - Experiment completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.d (Queue - throughput)
# -------------------------------
printf "\nCalculating the results of PB and PWF algorithms for Queue-throughput (Figure 1d).\n"
# Run experiments
# PBqueue
./bench.sh  pbqueue.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbcomb.tmp
# PWFqueue
./bench.sh  pwfqueue.run -t ${THREADS} -s ${STEP} -w 512 -b 22 | tee -a results/tmp/pwfcomb.tmp
# PBqueue-no-rec
./bench.sh  pbqueue_no_rec.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbcomb-no-rec.tmp
# Collect results
echo -e "threads\t PBqueue" > results/tmp/pbcomb_res.tmp
cat results/tmp/pbcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%s\t%7.2f\n", $1, $9}' >> results/tmp/pbcomb_res.tmp
echo -e "PWFqueue" > results/tmp/pwfcomb_res.tmp
cat results/tmp/pwfcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pwfcomb_res.tmp
echo -e "PBqueue-no-rec" > results/tmp/pbcomb-no-rec_res.tmp
cat results/tmp/pbcomb-no-rec.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pbcomb-no-rec_res.tmp
results_files_array=()
results_files_array+=("results/tmp/pbcomb_res.tmp")
results_files_array+=("results/tmp/pwfcomb_res.tmp")
results_files_array+=("results/tmp/pbcomb-no-rec_res.tmp")
paste -d"\t" ${results_files_array[@]} > results/figure1d.txt
rm -rf results/tmp/*
printf "Queue-throughput (Figure 1d) - Experiment completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.e (Queue - number of pwbs)
# -----------------------------------
printf "\nCalculating the results of PB and PWF algorithms for Queue-number-of-pwbs (Figure 1e).\n"
# Run experiments
# PBcomb
./bench.sh  pbqueue_pwbs.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbcomb.tmp
# PWFcomb
./bench.sh  pwfqueue_pwbs.run -t ${THREADS} -s ${STEP} -w 512 -b 22 | tee -a results/tmp/pwfcomb.tmp
# Collect results
echo -e "threads\t PBqueue" > results/tmp/pbcomb_res.tmp
cat results/tmp/pbcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%s\n", $1}' >> results/tmp/pbcomb_res1.tmp
cat results/tmp/pbcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep num_pwbs | awk '{printf "%7.2f\n", $4}' >> results/tmp/pbcomb_res2.tmp
results_files_array=()
results_files_array+=("results/tmp/pbcomb_res1.tmp")
results_files_array+=("results/tmp/pbcomb_res2.tmp")
paste -d"\t" ${results_files_array[@]} >> results/tmp/pbcomb_res.tmp
echo -e "PWFqueue" > results/tmp/pwfcomb_res.tmp
cat results/tmp/pwfcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep num_pwbs | awk '{printf "%7.2f\n", $4}' >> results/tmp/pwfcomb_res.tmp
results_files_array=()
results_files_array+=("results/tmp/pbcomb_res.tmp")
results_files_array+=("results/tmp/pwfcomb_res.tmp")
paste -d"\t" ${results_files_array[@]} > results/figure1e.txt
rm -rf results/tmp/*
printf "Queue-number-of-pwbs (Figure 1e) - Experiment completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.f (Queue - throughput with no pwbs)
# --------------------------------------------
printf "\nCalculating the results of PB and PWF algorithms for Queue-throughput-with-no-pwbs (Figure 1f).\n"
# Run experiments
# PBqueue
./bench.sh  pbqueue_no_pwbs.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbcomb.tmp
# PWFqueue
./bench.sh  pwfqueue_no_pwbs.run -t ${THREADS} -s ${STEP} -w 512 -b 22 | tee -a results/tmp/pwfcomb.tmp
# Collect results
echo -e "threads\t PBqueue" > results/tmp/pbcomb_res.tmp
cat results/tmp/pbcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%s\t%7.2f\n", $1, $9}' >> results/tmp/pbcomb_res.tmp
echo -e "PWFqueue" > results/tmp/pwfcomb_res.tmp
cat results/tmp/pwfcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pwfcomb_res.tmp
results_files_array=()
results_files_array+=("results/tmp/pbcomb_res.tmp")
results_files_array+=("results/tmp/pwfcomb_res.tmp")
paste -d"\t" ${results_files_array[@]} > results/figure1f.txt
rm -rf results/tmp/*
printf "Queue-throughput-with-no-pwbs (Figure 1f) - Experiment completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.g (Stack - throughput)
# -------------------------------
printf "\nCalculating the results of PB and PWF algorithms for Stack-throughput (Figure 1g).\n"
# Run experiments
# PBstack
./bench.sh  pbstack.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbcomb.tmp
# PWFstack
./bench.sh  pwfstack.run -t ${THREADS} -s ${STEP} -w 512 -b 21 | tee -a results/tmp/pwfcomb.tmp
# PBstack-no-rec
./bench.sh  pbstack_no_rec.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbcomb-no-rec.tmp
# PWFstack-no-rec
./bench.sh  pwfstack_no_rec.run -t ${THREADS} -s ${STEP} -w 512 -b 21 | tee -a results/tmp/pwfcomb-no-rec.tmp
# PBstack-no-elim
./bench.sh  pbstack_no_elim.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbcomb-no-elim.tmp
# PWFstack-no-elim
./bench.sh  pwfstack_no_elim.run -t ${THREADS} -s ${STEP} -w 512 -b 21 | tee -a results/tmp/pwfcomb-no-elim.tmp
# Collect results
echo -e "threads\t PBstack" > results/tmp/pbcomb_res.tmp
cat results/tmp/pbcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%s\t%7.2f\n", $1, $9}' >> results/tmp/pbcomb_res.tmp
echo -e "PWFstack" > results/tmp/pwfcomb_res.tmp
cat results/tmp/pwfcomb.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pwfcomb_res.tmp
echo -e "PBstack-no-rec" > results/tmp/pbcomb-no-rec_res.tmp
cat results/tmp/pbcomb-no-rec.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pbcomb-no-rec_res.tmp
echo -e "PWFstack-no-rec" > results/tmp/pwfcomb-no-rec_res.tmp
cat results/tmp/pwfcomb-no-rec.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pwfcomb-no-rec_res.tmp
echo -e "PBstack-no-elim" > results/tmp/pbcomb-no-elim_res.tmp
cat results/tmp/pbcomb-no-elim.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pbcomb-no-elim_res.tmp
echo -e "PWFstack-no-elim" > results/tmp/pwfcomb-no-elim_res.tmp
cat results/tmp/pwfcomb-no-elim.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pwfcomb-no-elim_res.tmp
results_files_array=()
results_files_array+=("results/tmp/pbcomb_res.tmp")
results_files_array+=("results/tmp/pwfcomb_res.tmp")
results_files_array+=("results/tmp/pbcomb-no-rec_res.tmp")
results_files_array+=("results/tmp/pwfcomb-no-rec_res.tmp")
results_files_array+=("results/tmp/pbcomb-no-elim_res.tmp")
results_files_array+=("results/tmp/pwfcomb-no-elim_res.tmp")
paste -d"\t" ${results_files_array[@]} > results/figure1g.txt
rm -rf results/tmp/*
printf "Stack-throughput (Figure 1g) - Experiment completed.\n"
# ==================================================================================================

# ==================================================================================================
# Figure 1.h (Heap - throughput)
# ------------------------------
printf "\nCalculating the results of PB and PWF algorithms for Heap-throughput (Figure 1f).\n"
# Run experiments
# PBheap-1024
./bench.sh  pbheap1024.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbheap1024.tmp
# PBheap-512
./bench.sh  pbheap512.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbheap512.tmp
# PBheap-256
./bench.sh  pbheap256.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbheap256.tmp
# PBheap-128
./bench.sh  pbheap128.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbheap128.tmp
# PBheap-64
./bench.sh  pbheap64.run -t ${THREADS} -s ${STEP} -w 512 | tee -a results/tmp/pbheap64.tmp
# Collect results
echo -e "threads\t PBheap-1024" > results/tmp/pbheap1024_res.tmp
cat results/tmp/pbheap1024.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%s\t%7.2f\n", $1, $9}' >> results/tmp/pbheap1024_res.tmp
echo -e "PBheap-512" > results/tmp/pbheap512_res.tmp
cat results/tmp/pbheap512.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pbheap512_res.tmp
echo -e "PBheap-256" > results/tmp/pbheap256_res.tmp
cat results/tmp/pbheap256.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pbheap256_res.tmp
echo -e "PBheap-128" > results/tmp/pbheap128_res.tmp
cat results/tmp/pbheap128.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pbheap128_res.tmp
echo -e "PBheap-64" > results/tmp/pbheap64_res.tmp
cat results/tmp/pbheap64.tmp | sed -r "s/\x1b\[[0-9;]*m?//g" | grep average | awk '{printf "%7.2f\n", $9}' >> results/tmp/pbheap64_res.tmp
results_files_array=()
results_files_array+=("results/tmp/pbheap1024_res.tmp")
results_files_array+=("results/tmp/pbheap512_res.tmp")
results_files_array+=("results/tmp/pbheap256_res.tmp")
results_files_array+=("results/tmp/pbheap128_res.tmp")
results_files_array+=("results/tmp/pbheap64_res.tmp")
paste -d"\t" ${results_files_array[@]} > results/figure1h.txt
rm -rf results/tmp/*
printf "Heap-throughput (Figure 1h) - Experiment completed.\n"
# ==================================================================================================