#!/bin/bash

ulimit -s unlimited

current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"

# []
for wl in 44 45; do
  for cfg in {1..40..2}; do
    "$exec" $cfg $wl
  done
done



