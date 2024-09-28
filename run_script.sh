#!/bin/bash
ulimit -s unlimited

# for cfg in {1..79..5}; do
#   /homes/yrayhan/works/erebus/build/bin/erebus $cfg
# done

for cfg in 40 42 43 44 45 47 49; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg
done