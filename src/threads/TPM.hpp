#pragma once

#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
// -------------------------------------------------------------------------------------
#include "oneapi/tbb/concurrent_priority_queue.h"
// -------------------------------------------------------------------------------------
#include "shared-headers/Units.hpp"
#include "scheduling/RM.hpp"
#include "scheduling/GM.hpp"
#include "shared-headers/PerfEvent.hpp"
#include "profiling/PerfCounters.hpp"
// -------------------------------------------------------------------------------------
using namespace erebus::storage::rtree;
namespace erebus
{
namespace tp
{
    
class TPManager{
  public:
    dm::GridManager *gm;
    scheduler:: ResourceManager *rm;
    // -------------------------------------------------------------------------------------    
    static const int MAX_MEGAMIND_THREADS = 10;
    static const int MAX_WORKER_THREADS = 10;
    static const int MAX_ROUTER_THREADS = 4;

    static const int CURR_MEGAMIND_THREADS = 2;
    static const int CURR_WORKER_THREADS = 4;
    static const int CURR_ROUTER_THREADS = 2;
    
    // -------------------------------------------------------------------------------------    
    std::vector<std::thread> glb_worker_thrds; 
    std::vector<std::thread> glb_megamind_thrds;
    std::vector<std::thread> glb_router_thrds; 
    // -------------------------------------------------------------------------------------
    static const u64 PERF_STAT_COLLECTION_INTERVAL = 100;
    // -------------------------------------------------------------------------------------
    struct MegaMindThread {
      u64 cpuid;
      std::mutex mutex;
      std::condition_variable cv;
      // oneapi::tbb::concurrent_priority_queue<std::function<void()>> jobs;
      
      bool wt_ready = true;   // Idle
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };
    
    struct WorkerThread {
      u64 cpuid;
      std::mutex mutex;
      std::condition_variable cv;
      oneapi::tbb::concurrent_priority_queue<Rectangle, Rectangle::compare_f> jobs;
      std::vector<PerfCounter> perf_stats;
      bool wt_ready = true;   // Idle
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };

    struct RouterThread {
      u64 cpuid;
      std::mutex mutex;
      std::condition_variable cv;
      // oneapi::tbb::concurrent_priority_queue<std::function<void()>> jobs;
      
      bool wt_ready = true;   // Idle
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };
    // -------------------------------------------------------------------------------------
    MegaMindThread megamind_threads_meta[MAX_MEGAMIND_THREADS];
    WorkerThread worker_threads_meta[MAX_WORKER_THREADS];
    RouterThread router_threads_meta[MAX_ROUTER_THREADS];
    // -------------------------------------------------------------------------------------
    TPManager(std::vector<int> megamind_cpuids, std::vector<int> worker_cpuids, std::vector<int> router_cpuids, dm::GridManager *gm, scheduler::ResourceManager *rm);
    //  ~TPManager();
    // -------------------------------------------------------------------------------------
};


}  // namespace tp 
}  // namespace erebus

