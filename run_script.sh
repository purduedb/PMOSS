#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"

for wl in 11; do
  for cfg in {1..30..3}; do
    "$exec" $cfg $wl
  done
  for cfg in {30..80..3}; do
    "$exec" $cfg $wl
  done
  for cfg in {80..100..3}; do
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
