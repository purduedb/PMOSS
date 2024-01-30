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
    
    std::vector<CPUID> router_cpuids; 
    std::vector<CPUID> worker_cpuids; 
    std::vector<CPUID> megamind_cpuids; 
    
    // -------------------------------------------------------------------------------------    
    static const int MAX_MEGAMIND_THREADS = 20;
    static const int MAX_WORKER_THREADS = 200;
    static const int MAX_ROUTER_THREADS = 20;

    static const int CURR_MEGAMIND_THREADS = 8;
    static const int CURR_WORKER_THREADS = 40;
    static const int CURR_ROUTER_THREADS = 8;
    
    // -------------------------------------------------------------------------------------    
    static const u64 PERF_STAT_COLLECTION_INTERVAL = 1;
    // -------------------------------------------------------------------------------------
    struct MegaMindThread {
      std::thread th;
      u64 cpuid;
      std::mutex mutex;
      std::condition_variable cv;
      
      bool running = true;
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };
    
    struct WorkerThread {
      std::thread th;
      u64 cpuid;
      std::mutex mutex;
      std::condition_variable cv;
      oneapi::tbb::concurrent_priority_queue<Rectangle, Rectangle::compare_f> jobs;
      
      // This is a local view of the data distribution 
      // TODO: the size should be equal to the max number of grids
      HWCounterStats shadowDataDist[1000];
      // std::vector<PerfCounter> perf_stats;


      bool running = true;
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };

    struct RouterThread {
      std::thread th;
      u64 cpuid;
      std::mutex mutex;
      std::condition_variable cv;
      // oneapi::tbb::concurrent_priority_queue<std::function<void()>> jobs;
      
      bool running = true;
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };
    struct StandbyThread {
      std::thread th;
      u64 cpuid;
      std::mutex mutex;
      std::condition_variable cv;
      // oneapi::tbb::concurrent_priority_queue<std::function<void()>> jobs;
      
      bool running = true;
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };
    // -------------------------------------------------------------------------------------
    std::unordered_map<CPUID, WorkerThread> glb_worker_thrds; 
    std::unordered_map<CPUID, MegaMindThread> glb_megamind_thrds; 
    std::unordered_map<CPUID, RouterThread> glb_router_thrds; 
    std::unordered_map<CPUID, StandbyThread> glb_standby_thrds; 

    // std::unordered_map<CPUID, std::thread> glb_worker_thrds; 
    // std::unordered_map<CPUID, std::thread> glb_megamind_thrds; 
    // std::unordered_map<CPUID, std::thread> glb_router_thrds; 
    
    // std::vector<std::thread> glb_worker_thrds; 
    // std::vector<std::thread> glb_megamind_thrds;
    // std::vector<std::thread> glb_router_thrds; 
    // -------------------------------------------------------------------------------------
    // MegaMindThread megamind_threads_meta[MAX_MEGAMIND_THREADS];
    // WorkerThread worker_threads_meta[MAX_WORKER_THREADS];
    // RouterThread router_threads_meta[MAX_ROUTER_THREADS];
    // -------------------------------------------------------------------------------------
    TPManager(std::vector<CPUID> megamind_cpuids, std::vector<CPUID> worker_cpuids, std::vector<CPUID> router_cpuids, dm::GridManager *gm, scheduler::ResourceManager *rm);
    void initWorkerThreads();
    void initRouterThreads();
    void initMegaMindThreads();

    void dumpGridHWCounters(int tID);
    void terminateWorkerThreads();
    // ~TPManager();
    // -------------------------------------------------------------------------------------
};


}  // namespace tp 
}  // namespace erebus

