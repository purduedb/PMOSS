#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"

# round=50
# cfg=(37)
# wl_span=1800000
# for wl in 12; do
#   for c in "${cfg[@]}"; do
#     "$exec" $c $wl $round $wl_span
#   done
# done

# round=60
# cfg=(100)
# wl_span=4800000
# for wl in 12; do
#   for c in "${cfg[@]}"; do
#     "$exec" $c $wl $round $wl_span
#   done
# done

# round=4
# cfg=(506)
# wl_span=1800000
# for wl in 12; do
#   for c in "${cfg[@]}"; do
#     "$exec" $c $wl $round $wl_span
#   done
# done


round=43
cfg=(506)
wl_span=1800000
for wl in 12; do
  for c in "${cfg[@]}"; do
    "$exec" $c $wl $round $wl_span
  done
done