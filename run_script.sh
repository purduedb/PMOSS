#!/bin/bash

sudo sh -c 'echo -1 >/proc/sys/kernel/perf_event_paranoid'
ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

for cfg in {1..59..3}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 34
done

for cfg in {1..59..3}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 35
done

for cfg in {1..59..3}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 36
done


# for cfg in {31..59..1}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 11
# done

# for cfg in {31..59..1}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 13
# done

# for cfg in {31..59..1}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 39
# done

# for cfg in {31..59..1}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 40
# done