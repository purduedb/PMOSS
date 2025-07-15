#!/bin/bash
ulimit -s unlimited

current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"
# 12 34 41 51 26 201
# 11 [37 40 40 7 200],
# 26 [100 48 54 506 206]
# 27 [100 101 58 506 207]
for wl in 12; do
  for cfg in 34 41 51 26; do
    "$exec" $cfg $wl
  done
done
# [11 12 44 45 16]
# [27]
# for wl in 28; do
#   for cfg in 100 101 102; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {1..30..4}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {30..40..4}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {40..50..4}; do
#     "$exec" $cfg $wl
#   done
#   for cfg in {50..59..4}; do
#     "$exec" $cfg $wl
#   done
# done

# Learned Models
# [11]
# for wl in 34; do
#   for cfg in 208; do
#     "$exec" $cfg $wl
#   done
# done


##############################################################################################################

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

