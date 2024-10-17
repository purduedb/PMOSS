#!/bin/bash

sudo sh -c 'echo -1 >/proc/sys/kernel/perf_event_paranoid'
ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1


for cfg in {1..29..1}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 12
done

for cfg in {1..29..1}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 16
done

for cfg in {1..29..1}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 32
done

for cfg in {1..29..1}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 41
done