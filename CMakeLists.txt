cmake_minimum_required(VERSION 3.5)

project("erebus"
    LANGUAGES C CXX
)

# Set output directories
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")

# Set compilation flags
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_COMPILE_FLAGS           "-g -O3 -W -Wall -pedantic -DLEVEL1_DCACHE_LINESIZE=${LEVEL1_DCACHE_LINESIZE} -DPAGESIZE=${PAGESIZE} -march=native -Wno-variadic-macros -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-label-as-value -Wno-vla-extension -Wl,--no-as-needed")
SET(CMAKE_C_FLAGS                 "${CMAKE_C_FLAGS} ${CMAKE_COMPILE_FLAGS}")
SET(CMAKE_CXX_FLAGS               "-std=c++17 ${CMAKE_CXX_FLAGS} ${CMAKE_COMPILE_FLAGS}")
SET(CMAKE_CXX_STANDARD_LIBRARIES  "-lnuma")

# Detect CPU vendor
execute_process(
  COMMAND bash -c "lscpu | grep 'Vendor ID' | awk '{print $3}'"
  OUTPUT_VARIABLE CPU_VENDOR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Detect Intel CPU and conditionally include PCM
if(CPU_VENDOR STREQUAL "GenuineIntel")
  add_definitions(-DINTEL_CPU=1)
  message(STATUS "Intel CPU detected (${CPU_VENDOR}) - enabling PCM support")
  if(EXISTS ${CMAKE_SOURCE_DIR}/third-party/pcm/src)
    include_directories(third-party/pcm)
  endif()
else()
  add_definitions(-DINTEL_CPU=0)
  message(STATUS "Non-Intel CPU detected (${CPU_VENDOR}) - PCM support disabled")
endif()

# Directories
include_directories(src)


find_package(TBB REQUIRED)


# Our Project
set(SOURCE_FILES
  src/threads/TPM.cc
  src/storage/rtree/rtree.cpp
  src/storage/qtree/qtree.cpp
  src/storage/btree/btree.cpp
  src/erebus.cpp
  src/utils/Misc.cpp
  src/scheduling/RM.cpp
  src/scheduling/GM.cpp
  src/utils/ZipfGenerator.cpp
  src/utils/RandomGenerator.cpp
  src/utils/ScrambledZipfGenerator.cpp
  src/utils/FNVHash.cpp
  src/ycsbc/generator.cpp
  src/ycsbc/core_workload.cpp
  src/ycsbc/acknowledged_counter_generator.cc
)

# Add PCM source only for Intel and if PCM exists
if(CPU_VENDOR STREQUAL "GenuineIntel" AND EXISTS ${CMAKE_SOURCE_DIR}/third-party/pcm/src)
  list(APPEND SOURCE_FILES src/profiling/PCMMem.cpp)
endif()

add_definitions(-DPROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Executables
add_executable(erebus ${SOURCE_FILES})
# Link libraries conditionally
set(LINK_LIBS TBB::tbb -fsanitize=address)

if(CPU_VENDOR STREQUAL "GenuineIntel" AND EXISTS ${CMAKE_SOURCE_DIR}/third-party/pcm/build/lib/libpcm.so)
  list(APPEND LINK_LIBS ${CMAKE_SOURCE_DIR}/third-party/pcm/build/lib/libpcm.so)
endif()

target_link_libraries(erebus ${LINK_LIBS})
