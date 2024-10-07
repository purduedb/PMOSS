#!/bin/bash
ulimit -s unlimited

for cfg in {4..79..3}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg
done
