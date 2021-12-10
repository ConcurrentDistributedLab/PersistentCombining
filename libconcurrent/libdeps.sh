#!/bin/bash

LDLIBS="-lpthread -latomic";

DEFINITIONS=(SYNCH_NUMA_SUPPORT SYNCH_TRACK_CPU_COUNTERS SYNCH_ENABLE_PERSISTENT_MEM);
LIBS=("-lnuma" "-lpapi" "-lpmem -lvmem");

for i in ${!DEFINITIONS[@]}; do
    if grep -xq "\s*#define\s\+${DEFINITIONS[i]}\(\s*\|\s.*\)" libconcurrent/config.h
    then
        LDLIBS="$LDLIBS ${LIBS[i]}";
    fi
done

echo $LDLIBS;