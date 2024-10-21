#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

for cfg in {2..49..2}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 11
done

for cfg in {2..49..2}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 39
done

for cfg in {2..49..2}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 40
done

for cfg in {2..49..2}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg 13
done




