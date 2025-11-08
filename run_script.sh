#!/bin/bash

ulimit -s unlimited

current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"

# [44 45 11 12 16 13]
# for wl in 12 16; do
#   for cfg in {2..39..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in 100;do
#     "$exec" $cfg $wl
#   done
# done


# learned 
#[11 12 16 44 45 1]
for wl in 11; do
  for cfg in 50000 50001; do
    "$exec" $cfg $wl
  done
done
