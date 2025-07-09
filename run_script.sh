#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"

# 16 baki, 11 of 100 baki
# [11, 12, 13, 44, 45]
for wl in 16; do
  for cfg in {1..2..3}; do
    "$exec" $cfg $wl
  done
  for cfg in {30..80..3}; do
    "$exec" $cfg $wl
  done
  for cfg in {80..100..3}; do
    "$exec" $cfg $wl
  done
  for cfg in 100 101 102; do
    "$exec" $cfg $wl
  done
done


# for wl in 12; do
# `for cfg in {1..50..2}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {50..100..3}; do
#     "$exec" $cfg $wl
#   done
# done
