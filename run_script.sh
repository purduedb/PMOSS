#!/bin/bash
ulimit -s unlimited

# for cfg in {1..79..3}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg
# done

numactl --interleave=0,1,2,3,4,5,6,7 /homes/yrayhan/works/erebus/build/bin/erebus 501
for cfg in 500 506 511; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg
done