#!/bin/bash

sudo sh -c 'echo -1 >/proc/sys/kernel/perf_event_paranoid'
ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

# [11 12 16 44 45 13]
current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"
# for wl in 12 16; do
#   for cfg in {2..30..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {31..40..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {41..50..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {51..59..3}; do
#     "$exec" $cfg $wl
#   done
#   # for cfg in 100 101 102; do
#   #   "$exec" $cfg $wl
#   # done
# done

# Learned Models []
cfg=201
wl=12
"$exec" $cfg $wl
