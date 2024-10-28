#!/bin/bash

ulimit -s unlimited
export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1


for wl in 0; do
  for cfg in {0..49..3}; do
    /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
  done
done

# for wl in 11; do
#   for cfg in 216 218; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 43; do
#   for cfg in 1; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 43; do
#   for cfg in 100 101 102 103; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 43; do
#   for cfg in {4..60..3}; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# For baselines
# for wl in 12 16 32 41 34 35 36; do
#   for cfg in 100 101 102 103; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 13; do
#   for cfg in 216 217 218; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 11; do
#   for cfg in 216 218; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 11; do
#   for cfg in {210..216..1}; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done
# for wl in 11; do
#   for cfg in {100..102..1}; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done
# for wl in 11; do
#   for cfg in {12..59..2}; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done


# for wl in 32; do
#   for cfg in {203..206..1}; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 41; do
#   for cfg in {203..206..1}; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done



# DOne only 100, 11
# for wl in 11 13; do
#   for cfg in 100 101 102 103; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 12; do
#   for cfg in {259..261..1}; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 41; do
#   for cfg in {253..255..1}; do
#     /homes/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done