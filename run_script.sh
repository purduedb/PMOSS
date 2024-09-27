#!/bin/bash
ulimit -s unlimited

# for cfg in {49..59..3}; do
#   /users/yrayhan/works/erebus/build/bin/erebus $cfg
# done

for cfg in 50 51 54 59 57; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg
done