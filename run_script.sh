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
# wl=(12 16)
# cfg=(12001 12004)
# for i in "${!wl[@]}"; do
#   "$exec" ${cfg[$i]} ${wl[$i]}
# done
# wl=11
# cfg=(1000 1001 1002 1003 1004 1005 1006 1007 1008)
# for c in "${cfg[@]}"; do
#   "$exec" $c $wl
# done


# wl=(11)
# cfg=(200)
# for i in "${!wl[@]}"; do
#   "$exec" ${cfg[$i]} ${wl[$i]}
# done


round=24
cfg=(-5)
for wl in 12; do
  for c in "${cfg[@]}"; do
    "$exec" $c $wl $round
  done
done