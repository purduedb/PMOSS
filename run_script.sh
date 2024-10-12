#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

# for cfg in {1..79..4}; do
#   /homes/yrayhan/works/erebus/build/bin/erebus $cfg
# done

for cfg in {1..49..3}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 34
done

for cfg in {1..49..3}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 35
done

for cfg in {1..49..3}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 35
done

# for cfg in 32 33 34 35 37 38 39; do
#   /homes/yrayhan/works/erebus/build/bin/erebus $cfg
# done