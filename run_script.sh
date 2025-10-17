#!/bin/bash
ulimit -s unlimited

export PCM_NO_MSR=1
export PCM_NMI_WATCHDOG=1

current_dir=$(pwd)
echo "Current directory: $current_dir"
exec="$current_dir/build/bin/erebus"



# NVIDIA [44 45 12 16 11 13]
# for wl in 13; do
#   numactl --interleave=0,2,3,4,5,6,7 "$exec" 501 $wl
#   for cfg in 500 502 506; do
#     "$exec" $cfg $wl
#   done
# done


# INTEL SKX 4S4N [44 45 11 12 16 13]
# for wl in 13; do
#   numactl --interleave=0,2,4,6 "$exec" 501 $wl
#   for cfg in 500 502 506; do
#     "$exec" $cfg $wl
#   done
# done

round=5
for wl in 11; do
  # numactl --interleave=0,2,4,6 "$exec" 501 $wl
  # for cfg in 500 502 506; do
  #   "$exec" $cfg $wl
  # done
for cfg in 500 502 506; do
  "$exec" $cfg $wl $round
done
numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 $wl $round
done

# AMD EPYC 7543 2S_8N [11, 12, 13, 44, 45, 16]
# INTEL SKX [44 45 13 12 16 11]
# for wl in ; do
#   numactl --interleave=0,1,2,3,4,5,6,7 "$exec" 501 $wl
#   for cfg in 500 502 506; do
#     "$exec" $cfg $wl
#   done
# done

# INTEL SB [44 45 11]
# for wl in 44 45; do
#   numactl --interleave=0,1,2,3 "$exec" 501 $wl
#   for cfg in 500 502 506; do
#     "$exec" $cfg $wl
#   done
# done

# AMD EPYC 7543 2S_2N [11, 12, 13, 44, 45, 16]
# Intel ICE [44 45 12 16 13]
# for wl in 13; do
#   numactl --interleave=0,1 "$exec" 501 $wl
#   for cfg in 500 502 506; do
#     "$exec" $cfg $wl
#   done
# done


