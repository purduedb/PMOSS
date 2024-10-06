#!/bin/bash
ulimit -s unlimited

for cfg in {20..50..1}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg
done
