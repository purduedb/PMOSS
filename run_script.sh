#!/bin/bash
ulimit -s unlimited

current_dir=$(pwd)
echo "Current directory: $current_dir"
exec="$current_dir/build/bin/erebus"

# for wl in 12 16 32 41 34 35 36; do
#   numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 $wl
#   for cfg in 500 502 506; do
#     "$exec" $cfg $wl
#   done
# done

for wl in 11; do
  for cfg in 500; do
    "$exec" $cfg $wl
  done
done