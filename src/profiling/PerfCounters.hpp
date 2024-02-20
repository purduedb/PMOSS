#pragma once
#include <immintrin.h>
// -------------------------------------------------------------------------------------
#include "shared-headers/PerfEvent.hpp"
#include "PCMMem.hpp"
// -------------------------------------------------------------------------------------
#define QUERY_THRESHOLD_INS 9
#define QUERY_THRESHOLD_ACC 9

namespace erebus{
    
struct PerfCounter
{
    alignas(64) double raw_counter_values[PERF_EVENT_CNT] = {0};
    int normalizationConstant = 1;
    erebus::storage::rtree::Rectangle rscan_query;
    int gIdx;
    int result;
    int qType;
    PerfCounter(){

    }
    PerfCounter (const PerfCounter& pc){
        for(auto i = 0; i < PERF_EVENT_CNT; i++)
            raw_counter_values[i] = pc.raw_counter_values[i];
        rscan_query = pc.rscan_query;
        gIdx = pc.gIdx;
        result = pc.result;
        qType = pc.qType;
    }
};
struct HWCounterStats{
    // TODO: This nees to be thread safe, hence use maybe concurrent_vector?
    std::vector<PerfCounter> perf_stats;
};

struct DataDistSnap{
    __m512d rawQCounter[MAX_GRID_CELL][int(PERF_EVENT_CNT/8)+1];  // This gets copied to the main array
    double rawCntSamples[MAX_GRID_CELL] = {0};   
};

struct QueryViewSnap{
    int corrQueryReel[MAX_GRID_CELL][MAX_GRID_CELL] = {0};
};

struct QueryExecSnap{
    int qExecutedMice[MAX_GRID_CELL] = {0};
    int qExecutedElephant[MAX_GRID_CELL] = {0};
    int qExecutedMammoth[MAX_GRID_CELL] = {0};
};

struct TestInterferenceStats{
    std::vector<double> cycles;
    std::vector<double> results;
};

enum QUERY_TYPE{
    QUERY_MICE, QUERY_ELEPHANT, QUERY_MAMMOTH, SYNC_TOKEN
};

enum WKLOAD_DIST{
    UNIFORM, SINGLE_HOTSPOT, FOUR_HOTSPOT
};

struct IntelPCMCounter
{
    memdata_t sysParams;
    int UPIUtilize[max_sockets][max_qpi];
    int qType;

    // You will need to write copy constructor for
};

}  // namespace erebus