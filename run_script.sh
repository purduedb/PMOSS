#!/bin/bash
# ulimit -s unlimited

# for bs in 512 1024 2048 4096 8192 16384 32768; do
#   /users/yrayhan/PMOSS/build/bin/erebus 3 512 
# done
# for r in 0.01 0.2 0.5; do
# for r in 0.2; do
#   for wl in 49; do
#     for mode in 0 1 2; do
#       for bs in 64 128 256 512 1024 2048 4096; do
#         /users/yrayhan/PMOSS/build/bin/erebus_custom $wl $mode $bs $r
#       done
#     done
#   done 
# done

for r in 0.25 0.5; do
  for wl in 48 49 50; do
    /users/yrayhan/PMOSS/build/bin/erebus_native3 $wl -1 512 $r
    for mode in 0 1 2; do
      /users/yrayhan/PMOSS/build/bin/erebus_custom3 $wl $mode 512 $r
    done
  done 
done

# Baselines:Run the read workloads
# for wl in 12 16 32 41 34 35 36; do
#   for cfg in 100 101 102 103; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# Baselines:Run the write workloads 
# for wl in 11 39 40 13; do
#   for cfg in 100 101 102 103; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done


# for wl in 12 32 41; do
#   for cfg in 200; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 41; do
#   for cfg in {259..261..1}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 46; do
#   for cfg in 100 1 101 102 1 5 7; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done
# for wl in 31; do
#   for cfg in 1 3 5 100 101 102; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 31; do
#   for cfg in {259..264..1}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 11; do
#   for cfg in 100 101 102 103; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 11; do
#   for cfg in {1..59..2}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 46; do
#   for cfg in {260..262..1}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 45; do
#   for cfg in {10..60..3}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 41; do
#   for cfg in {253..255..1}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 39, 40; do
#   for cfg in {200..203..1}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 35; do
#   for cfg in {234..237..1}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 35; do
#   for cfg in 234 237; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 42; do
#   for cfg in 100 101 102 103; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 43; do
#   for cfg in {4..60..3}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done
# for wl in 43; do
#   for cfg in 1; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 43; do
#   for cfg in 100 101 102 103; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

# for wl in 11 13; do
#   for cfg in {2..69..3}; do
#     /users/yrayhan/works/erebus/build/bin/erebus $cfg $wl
#   done
# done

