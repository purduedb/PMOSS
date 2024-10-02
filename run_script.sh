#!/bin/bash
ulimit -s unlimited

for cfg in {33..79..2}; do
  /users/yrayhan/works/erebus/build/bin/erebus $cfg
done
