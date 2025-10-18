#!/bin/bash
ulimit -s unlimited

current_dir=$(pwd)
exec="$current_dir/build/bin/erebus"


# []
# []
# for wl in 12; do
#   # for cfg in 100 101 102; do
#   #   "$exec" $cfg $wl
#   # done
#   # for cfg in {2..30..4}; do
#   #   "$exec" $cfg $wl
#   # done
#   # for cfg in {31..40..4}; do
#   #   "$exec" $cfg $wl
#   # done
#   # for cfg in {42..50..4}; do
#   #   "$exec" $cfg $wl
#   # done
#   # for cfg in {51..59..4}; do
#   #   "$exec" $cfg $wl
#   # done
# done

# Learned Models
# [11]
# for wl in 11 44 45; do
#   for cfg in 11000 11003 11004; do
#     "$exec" $cfg $wl
#   done
# done

# wl=(11 11 45 45 44 44)
# cfg=(14000 15000 14003 15003 14002 15002)
# for i in "${!wl[@]}"; do
#   "$exec" "${cfg[$i]}" "${wl[$i]}"
# done

# wl=(12 12 16 16)
# cfg=(14001 15001 14004 15004)
# for i in "${!wl[@]}"; do
#   "$exec" "${cfg[$i]}" "${wl[$i]}"
# done

wl=(27 27)
cfg=(15006 14006)
for i in "${!wl[@]}"; do
  "$exec" "${cfg[$i]}" "${wl[$i]}"
done

# wl=(28 28)
# cfg=(15007 14007)
# for i in "${!wl[@]}"; do
#   "$exec" "${cfg[$i]}" "${wl[$i]}"
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

