#!/bin/bash

result_file="./results.txt"
script_path=$(dirname ${BASH_SOURCE[0]})

# Remove the resuts file if exists
if [ -f $result_file ] ; then
    rm $result_file
fi

# Get a string with all available benchmarks
benchmarks_string=$($script_path/bench.sh -l)

# Put the benchmarks into an array
benchmark_array=($(echo $benchmarks_string | tr " " "\n"))

# Run all the benchmarks with default values and output results to file
number_of_benchmarks=${#benchmark_array[@]}
for i in ${!benchmark_array[@]}
do
   index=$((i+1))
   printf "\nExecuting ${benchmark_array[$i]} ($index/$number_of_benchmarks)" | tee -a $result_file
   printf "\n========================================\n" >> $result_file
   $script_path/bench.sh ${benchmark_array[$i]} >> $result_file
   printf "\n\n" >> $result_file
   printf " ...Done\n"
done

printf "You can find the results in file %s\n" $result_file
