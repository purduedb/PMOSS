#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1


for wl in 0; do
  for cfg in {1..30..1}; do
    /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
  done
done
