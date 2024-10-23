#!/bin/bash
ulimit -s unlimited

# Baselines:Run the read workloads
# for wl in 12 16 32 41 34 35 36; do
#   for cfg in 100 101 102 103; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# Baselines:Run the write workloads 
# for wl in 11 39 40 13; do
#   for cfg in 100 101 102 103; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done


# for wl in 12 32 41; do
#   for cfg in 200; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

for wl in 12; do
  for cfg in 202; do
    /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
  done
done

# for wl in 11 13; do
#   for cfg in {2..69..3}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

