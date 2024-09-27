#!/bin/bash
ulimit -s unlimited

for cfg in {1..79..5}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg
done

# for cfg in 51 52 53 57 58 71 72; do
#   /homes/yrayhan/works/erebus/build/bin/erebus $cfg
# done