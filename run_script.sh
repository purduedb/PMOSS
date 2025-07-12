#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"

# [44 45 12 16 11 13]
# for wl in 11; do
#   for cfg in {2..30..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {31..40..3}; do
#     "$exec" $cfg $wl
#   done
#     for cfg in {41..50..3}; do
#     "$exec" $cfg $wl
#   done
#     for cfg in {51..59..3}; do
#     "$exec" $cfg $wl
#   done
#   # for cfg in 100 101 102; do
#   #   "$exec" $cfg $wl
#   # done
# done

# Learned Models
# [11 44 45 12 16]
for wl in 11; do
  for cfg in 200; do
    "$exec" $cfg $wl
  done
done
