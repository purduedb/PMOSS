#!/bin/bash
ulimit -s unlimited

current_dir=$(pwd)
echo "Current directory: $current_dir"
exec="$current_dir/build/bin/erebus"

# numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 12
# for cfg in 500 502 506; do
#   "$exec" $cfg 12
# done

# numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 16
# for cfg in 500 502 506; do
#   "$exec" $cfg 16 
# done

# numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 32
# for cfg in 500 502 506; do
#   "$exec" $cfg 32
# done

# numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 34
# for cfg in 500 502 506; do
#   "$exec" $cfg 34
# done

# numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 35
# for cfg in 500 502 506; do
#   "$exec" $cfg 35 
# done

# numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 36
# for cfg in 500 502 506; do
#   "$exec" $cfg 36
# done

# numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 41
# for cfg in 500 502 506; do
#   "$exec" $cfg 36
# done

numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 11
for cfg in 500 502 506; do
  "$exec" $cfg 11
done

numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 13
for cfg in 500 502 506; do
  "$exec" $cfg 13 
done

numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 39
for cfg in 500 502 506; do
  "$exec" $cfg 39
done

numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 40
for cfg in 500 502 506; do
  "$exec" $cfg 40
done
