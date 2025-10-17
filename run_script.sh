#!/bin/bash

sudo sh -c 'echo -1 >/proc/sys/kernel/perf_event_paranoid'
ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

# [11 12 16 44 45 13]
current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"
# for wl in 12 16; do
#   for cfg in {2..30..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {31..40..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {41..50..3}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {51..59..3}; do
#     "$exec" $cfg $wl
#   done
#   # for cfg in 100 101 102; do
#   #   "$exec" $cfg $wl
#   # done
# done

# Learned Models [11 12 44 45 16 ]
# cfg=202
# wl=44
# "$exec" $cfg $wl
# wl=13
# for cfg in 215;do
#   "$exec" $cfg $wl
# done


# wl=(11 11 11 45 45 45)
# cfg=(12000 13000 11000 12003 13003 11004)
# for i in "${!wl[@]}"; do
#   "$exec" ${cfg[$i]} ${wl[$i]}
# done

# wl=(11 45)
# cfg=(14000 14003)
# for i in "${!wl[@]}"; do
#   "$exec" ${cfg[$i]} ${wl[$i]}
# done

wl=(11 44 45)
cfg=(15000 15002 15003)
for i in "${!wl[@]}"; do
  "$exec" ${cfg[$i]} ${wl[$i]}
done

# wl=(12 16 12 16 12 16)
# cfg=(12001 12004 13001 13004 11001 11002)
# for i in "${!wl[@]}"; do
#   "$exec" ${cfg[$i]} ${wl[$i]}
# done