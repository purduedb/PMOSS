#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"

# for wl in 12; do
  for cfg in {1..30..1}; do
    "$exec" $cfg $wl
  done
# done
