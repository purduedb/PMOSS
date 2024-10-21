#!/bin/bash
ulimit -s unlimited



for wl in 12 16 32 41 34 35 36; do
  for cfg in 100 101 102 103; do
    /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
  done
done
# 101 41

# # for cfg in {1..59..2}; do
# #   /users/yrayhan/works/erebus/build/bin/erebus $cfg 11
# # done
# for cfg in {27..59..2}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 13
# done
# for cfg in {1..59..2}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 39
# done
# for cfg in {1..59..2}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 40
# done

