#!/bin/bash
ulimit -s unlimited
for cfg in {40..79..5}; do
  /homes/yrayhan/works/erebus/build/bin/erebus $cfg
done