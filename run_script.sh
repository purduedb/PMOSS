#!/bin/bash
# ulimit -s unlimited
# test run 
mode=0
bs=64
for r in 0.25; do
  for wl in 48; do
    /users/yrayhan/PMOSS/build/bin/erebus $wl -1 512 $r >> "results/adms_results.txt"
    # /users/yrayhan/PMOSS/build/bin/erebus_custom $wl $mode 512 $r >> "results/adms_results.txt"
    # /users/yrayhan/PMOSS/build/bin/erebus $wl $mode $bs $r >> "results/adms_results.txt"
  done 
done

# # run baseline: move_pages
# for r in 0.01 0.25 0.5; do
#   for wl in 48 49 50; do
#     /users/yrayhan/PMOSS/build/bin/erebus_native $wl -1 512 $r
#   done 
# done

# run move_pages2
# for r in 0.01 0.25 0.5; do
#   for wl in 48 49 50; do
#     for mode in 0 1 2; do
#       /users/yrayhan/PMOSS/build/bin/erebus_custom $wl $mode 512 $r
#     done
#   done 
# done

# for bs in 512 1024 2048 4096 8192 16384 32768; do
#   /users/yrayhan/PMOSS/build/bin/erebus 3 512 
# done