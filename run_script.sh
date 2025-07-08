#!/bin/bash
ulimit -s unlimited

# export PCM_NO_MSR=1
# export PCM_NMI_WATCHDOG=1

current_dir=$(pwd)
echo "Current directory: $current_dir"
exec="$current_dir/build/bin/erebus"

# AMD EPYC 7543 2S_8N [11, 12, 13, 44, 45]
for wl in 16 12; do
  numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 $wl
  for cfg in 500 502 506; do
    "$exec" $cfg $wl
  done
done


# AMD EPYC 7543 2S_2N [11, 12, 13, 44, 45]
# for wl in 16; do
#   numactl --interleave=0,1 "$exec" 501 $wl
#   for cfg in 500 502 506; do
#     "$exec" $cfg $wl
#   done
# done

# Intel SKX
# for wl in 11; do
#   numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 $wl
#   for cfg in 500 502 506; do
#     "$exec" $cfg $wl
#   done
# done

# for wl in 11; do
#   for cfg in 500; do
#     "$exec" $cfg $wl
#   done
# done