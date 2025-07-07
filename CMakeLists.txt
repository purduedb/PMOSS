cmake_minimum_required(VERSION 3.5)

project("erebus" LANGUAGES C CXX)

# Set output directories
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")

# Set compilation flags
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_CXX_STANDARD 17)

# Add AddressSanitizer flags
set(SANITIZER_FLAGS "-fsanitize=address -g")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS} -O3 -W -Wall -pedantic -DLEVEL1_DCACHE_LINESIZE=${LEVEL1_DCACHE_LINESIZE} -DPAGESIZE=${PAGESIZE} -march=native -Wno-variadic-macros -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-label-as-value -Wno-vla-extension -Wl,--no-as-needed")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS} -O3 -W -Wall -pedantic -DLEVEL1_DCACHE_LINESIZE=${LEVEL1_DCACHE_LINESIZE} -DPAGESIZE=${PAGESIZE} -march=native -Wno-variadic-macros -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-label-as-value -Wno-vla-extension -Wl,--no-as-needed")

set(CMAKE_CXX_STANDARD_LIBRARIES "-lnuma")

# Third party
include_directories(third-party/pcm)

# Directories
include_directories(src)

find_package(TBB REQUIRED)

# Source files
set(SOURCE_FILES
  src/threads/TPM.cc
  src/storage/rtree/rtree.cpp
  src/storage/qtree/qtree.cpp
  src/storage/btree/btree.cpp
  src/erebus.cpp
  src/utils/Misc.cpp
  src/scheduling/RM.cpp
  src/scheduling/GM.cpp
  src/profiling/PCMMem.cpp
  src/utils/ZipfGenerator.cpp
  src/utils/RandomGenerator.cpp
  src/utils/ScrambledZipfGenerator.cpp
  src/utils/FNVHash.cpp
  src/ycsbc/generator.cpp
  src/ycsbc/core_workload.cpp
  src/ycsbc/acknowledged_counter_generator.cc
)

add_definitions(-DPROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Executable
add_executable(erebus ${SOURCE_FILES})

# Link AddressSanitizer and other libraries
target_link_libraries(erebus 
  TBB::tbb
  ${CMAKE_SOURCE_DIR}/third-party/pcm/build/lib/libpcm.so
  -fsanitize=address
)
