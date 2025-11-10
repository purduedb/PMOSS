#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"

# [44 45]
# for wl in 45; do
#   for cfg in {1..2..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {30..80..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {80..100..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in 100 101 102; do
#     "$exec" $cfg $wl
#   done
# done


# Learned Models
# [12 11 44 45 13 16]
# for wl in 13; do
#   for cfg in 205; do
#     "$exec" $cfg $wl
#   done
# done

# w / wo profiling
# [11]
# for wl in 16; do
#   for cfg in 204; do
#     "$exec" $cfg $wl
#   done
# done

# Date: 2024-06-10 (Testing new KBs)
# Add a wl list and a cfg list 
# wl=(11 11 11 11)
# cfg=(39 48 57 16)
# for i in "${!wl[@]}"; do
#   "$exec" ${cfg[$i]} ${wl[$i]}
# done

wl=(11)
cfg=(70000)
for i in "${!wl[@]}"; do
  "$exec" ${cfg[$i]} ${wl[$i]}
done
