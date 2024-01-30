#pragma once
// -------------------------------------------------------------------------------------
#include "shared-headers/PerfEvent.hpp"
// -------------------------------------------------------------------------------------
#define QUERY_THRESHOLD_INS 9
#define QUERY_THRESHOLD_ACC 9

namespace erebus{
    
struct PerfCounter
{
    double raw_counter_values[PERF_EVENT_CNT];
    int normalizationConstant = 1;
    erebus::storage::rtree::Rectangle rscan_query;
    int result;
};
struct HWCounterStats{
    // TODO: This nees to be thread safe, hence use maybe concurrent_vector?
    std::vector<PerfCounter> perf_stats;
};

enum QUERY_TYPE{
    QUERY_MICE, QUERY_ELEPHANT, QUERY_MAMMOTH
};

struct IntelPCMCounter
{
    
};

}  // namespace erebus