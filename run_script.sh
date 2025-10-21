#!/bin/bash
ulimit -s unlimited

export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

current_dir=$(pwd)
echo "Current directory: $current_dir"
exec="$current_dir/build/bin/erebus"


round=5
wl_span=1800000
for wl in 12; do
  for cfg in 500 502 506; do
    "$exec" $cfg $wl $round $wl_span
  done
  numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 $wl $round $wl_span
done
