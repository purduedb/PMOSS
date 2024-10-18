#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

for cfg in {1..49..2}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 12
done

for cfg in {1..49..2}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 16
done

for cfg in {1..49..2}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 32
done

for cfg in {1..49..2}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 41
done




