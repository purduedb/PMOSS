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
for wl in 13; do
  for cfg in 205; do
    "$exec" $cfg $wl
  done
done