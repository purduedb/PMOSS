#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

# For baselines
for wl in 12 16 32 41 34 35 36; do
  for cfg in 100 101 102 103; do
    /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
  done
done

# for wl in 11 39 40 13; do
#   for cfg in 100 101 102 103; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for cfg in {2..49..2}; do
#   /homes/yrayhan/works/erebus/build/bin/erebus $cfg 11
# done

# for cfg in {2..49..2}; do
#   /homes/yrayhan/works/erebus/build/bin/erebus $cfg 39
# done

# for cfg in {2..49..2}; do
#   /homes/yrayhan/works/erebus/build/bin/erebus $cfg 40
# done

# for cfg in {2..49..2}; do
#   /homes/yrayhan/works/erebus/build/bin/erebus $cfg 13
# done




