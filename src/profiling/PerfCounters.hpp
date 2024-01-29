#pragma once
// -------------------------------------------------------------------------------------
#include "shared-headers/PerfEvent.hpp"
// -------------------------------------------------------------------------------------

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

struct IntelPCMCounter
{
    
};

}  // namespace erebus