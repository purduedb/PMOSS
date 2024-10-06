#ifndef PMOSS_UNITS_H_
#define PMOSS_UNITS_H_
/*
* Taken from Leanstore
*/

#pragma once
// -------------------------------------------------------------------------------------
#include <stddef.h>
#include <stdint.h>

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
// -------------------------------------------------------------------------------------
using std::atomic;
using std::cerr;
using std::cout;
using std::endl;
using std::make_unique;
using std::string;
using std::to_string;
using std::tuple;
using std::unique_ptr;
// -------------------------------------------------------------------------------------
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = unsigned __int128;
// -------------------------------------------------------------------------------------
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
// -------------------------------------------------------------------------------------
using SIZE = size_t;
using PID = u64;
using LID = u64;   // Log ID
using TTS = u64;   // Transaction Time Stamp
using DTID = s64;  // Datastructure ID
using CPUID = s64;
using NUMAID = s16;
// -------------------------------------------------------------------------------------
using WORKERID = u16;
using TXID = u64;
using COMMANDID = u32;
#define TYPE_MSB(TYPE) (1ull << ((sizeof(TYPE) * 8) - 1))
// -------------------------------------------------------------------------------------
using TINYINT = s8;
using SMALLINT = s16;
using INTEGER = s32;
using UINTEGER = u32;
using DOUBLE = double;
using STRING = string;
using BITMAP = u8;
// -------------------------------------------------------------------------------------
using str = std::string_view;
// -------------------------------------------------------------------------------------
using BytesArray = std::unique_ptr<u8[]>;
// -------------------------------------------------------------------------------------
template <int s>
struct getTheSizeOf;
// -------------------------------------------------------------------------------------
constexpr u64 LSB = u64(1);
constexpr u64 MSB = u64(1) << 63;
constexpr u64 MSB_MASK = ~(MSB);
constexpr u64 MSB2 = u64(1) << 62;
constexpr u64 MSB2_MASK = ~(MSB2);
// -------------------------------------------------------------------------------------
// These are btree index framework
typedef uint64_t keytype;
typedef std::less<uint64_t> keycomp;
// These are workload operations
enum {
  OP_INSERT,
  OP_READ,
  OP_UPSERT,
  OP_SCAN,
};

// These are YCSB workloads
enum {
  WORKLOAD_A,
  WORKLOAD_C,
  WORKLOAD_E,
};

// These are key types we use for running the benchmark
enum {
  RAND_KEY,
  MONO_KEY,
  RDTSC_KEY,
  EMAIL_KEY,
};
// -------------------------------------------------------------------------------------
// Different workloads 
//  MD = MULTI-DIMENSIONAL, SD = SINGLE-DIMENSIONAL
//  
enum {
  MD_RS_UNIFORM, 
  MD_RS_NORMAL, 
  MD_LK_UNIFORM, 
  MD_RS_ZIPF, 
  MD_RS_HOT3, 
  MD_RS_HOT5, 
  MD_RS_HOT7, 
  MD_LK_RS_25_75, 
  MD_LK_RS_50_50, 
  MD_LK_RS_75_25, 
  MD_RS_LOGNORMAL, 
  SD_YCSB_WKLOADA,
  SD_YCSB_WKLOADC,
  SD_YCSB_WKLOADE,
  SD_YCSB_WKLOADF,
  SD_YCSB_WKLOADE1,
  SD_YCSB_WKLOADH,
  SD_YCSB_WKLOADI,
  WIKI_WKLOADA,
  WIKI_WKLOADC,
  WIKI_WKLOADE,
  WIKI_WKLOADF,
  WIKI_WKLOADI,
  WIKI_WKLOADH,
  WIKI_WKLOADA1,
  WIKI_WKLOADA2,
  WIKI_WKLOADA3,
  OSM_WKLOADA,
  OSM_WKLOADC,
  OSM_WKLOADE,
  OSM_WKLOADH,
  SD_YCSB_WKLOADA1,
  SD_YCSB_WKLOADH1,
};

enum {
  OSM_USNE,
  GEOLITE,
  BERLINMOD02,
  YCSB,
  WIKI,
  FB,
  OSM_CELLIDS,
};

enum {
  BTREE,
  RTREE,
  QUADTREE
};

enum{
  INTEL_SKX_8N_4S,
  INTEL_ICX_2N_2S,
  AMD_EPYC_7543_2N_2S
};
#endif