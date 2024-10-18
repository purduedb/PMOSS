#!/bin/bash
ulimit -s unlimited

for cfg in {1..49..2}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 12
done

for cfg in {1..49..2}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 16
done

for cfg in {1..49..2}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 32
done

for cfg in {1..49..2}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg 41
done

