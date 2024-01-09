#include "shared-headers/PerfEvent.hpp"

namespace erebus{
struct PerfCounter
{
    double raw_counter_values[PERF_EVENT_CNT];
    int normalizationConstant = 1;
};

struct IntelPCMCounter
{

};

}  // namespace erebus