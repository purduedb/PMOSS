#!/bin/bash
ulimit -s unlimited

for cfg in {2..79..1}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 12
done

for cfg in {2..79..1}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 16
done

for cfg in {2..79..1}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 32
done

for cfg in {2..79..1}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 41
done




# for cfg in {3..49..3}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg 35
# done



