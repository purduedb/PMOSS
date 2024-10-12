#!/bin/bash
ulimit -s unlimited

for cfg in {1..49..3}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 11
done

for cfg in {1..49..3}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 39
done

for cfg in {1..49..3}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 40
done


