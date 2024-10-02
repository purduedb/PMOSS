#!/bin/bash
ulimit -s unlimited

for cfg in {1..79..3}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg
done
