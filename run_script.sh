#!/bin/bash

sudo sh -c 'echo -1 >/proc/sys/kernel/perf_event_paranoid'
ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1


# for wl in 12 16 32 41 34 35 36; do
#   for cfg in 100 101 102 103; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 11 39 40 13; do
#   for cfg in 100 101 102 103; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

for wl in 13; do
  for cfg in 100 101 102 103; do
    /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
  done
done

# for cfg in {57..59..3}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 34
# done

# for cfg in {1..59..3}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 35
# done

# for cfg in {1..59..3}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 36
# done

# ---------------

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