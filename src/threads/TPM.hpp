#pragma once

#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <random>
// -------------------------------------------------------------------------------------
#include <immintrin.h>
#include <sys/stat.h>
#include <sys/types.h>
// -------------------------------------------------------------------------------------
#include "oneapi/tbb/concurrent_priority_queue.h"
#include "oneapi/tbb/concurrent_queue.h"
// -------------------------------------------------------------------------------------
#include "shared-headers/Units.hpp"
#include "scheduling/RM.hpp"
#include "scheduling/GM.hpp"
#include "shared-headers/PerfEvent.hpp"
#include "profiling/PerfCounters.hpp"
// -------------------------------------------------------------------------------------
#define WKLOAD 1
#define CONFIG 10 // 
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
    std::vector<CPUID> sys_sweeper_cpuids; 
    std::vector<CPUID> ncore_sweeper_cpuids; 
    
    // -------------------------------------------------------------------------------------    
    static const int MAX_NCORE_SWEEPER_THREADS = 20;
    static const int MAX_SYS_SWEEPER_THREADS = 20;
    static const int MAX_MEGAMIND_THREADS = 20;
    static const int MAX_WORKER_THREADS = 200;
    static const int MAX_ROUTER_THREADS = 20;
    
    static const int CURR_NCORE_SWEEPER_THREADS = 8;
    static const int CURR_SYS_SWEEPER_THREADS = 1;
    static const int CURR_MEGAMIND_THREADS = 8;
    static const int CURR_WORKER_THREADS = 56;
    static const int CURR_ROUTER_THREADS = 8;
    
    // -------------------------------------------------------------------------------------    
    static const u64 PERF_STAT_COLLECTION_INTERVAL = 1;
    // -------------------------------------------------------------------------------------
    
    struct SysSweeperThread {
      std::thread th;
      u64 cpuid;
      

      oneapi::tbb::concurrent_queue<IntelPCMCounter> pcmCounters;
      
      bool running = true;
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };

    struct NodeCoreSweeperThread {
      std::thread th;
      u64 cpuid;
      
      
      /**
       * TODO:  1. Data Distribution (A single snap: that is accumulated over time)
       *        2. Correlation matrix (Just a sinlgle snap)
       *        3. DRAM or Memory Resource Usage (A single snap: that is accumulated over time)
       *            [Only the one of socket 0 has this]
       * 
       * */ 
      // 
      vector <DataDistSnap> dataDistReel;
      vector<QueryViewSnap> queryViewReel;
      vector<memdata_t> DRAMResUsageReel;
      vector<QueryExecSnap> queryExecReel; 
      // int corrQueryReel[MAX_GRID_CELL][MAX_GRID_CELL] = {0};
      
      

      bool running = true;
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };

    
    struct MegaMindThread {
      std::thread th;
      u64 cpuid;
      
      
      bool running = true;
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };
    
    struct WorkerThread {
      std::thread th;
      u64 cpuid;
      
      oneapi::tbb::concurrent_priority_queue<Rectangle, Rectangle::compare_f> jobs;
      
      // This is a local view of the data distribution 
      // TODO: the size should be equal to the max number of grids
      // These are not needed, there because have them used in a different function
      HWCounterStats shadowDataDist[1000];
      HWCounterStats stats_;

      oneapi::tbb::concurrent_queue<PerfCounter> perf_stats;  // This is what we are currently using
      std::unordered_map<CPUID, u64> qExecutedMice;  // Grid Id to Mice Count
      std::unordered_map<CPUID, u64> qExecutedElephant;
      std::unordered_map<u64, u64> qExecutedMammoth;
      // int qExecutedMice[MAX_GRID_CELL] = {0};
      // int qExecutedElephant[MAX_GRID_CELL] = {0};
      // int qExecutedMammoth[MAX_GRID_CELL] = {0};
    
      bool running = true;
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };

    struct RouterThread {
      std::thread th;
      u64 cpuid;
      
      int qCorrMatrix[MAX_GRID_CELL][MAX_GRID_CELL] = {0};  //It needs to be thread-safe
      
      bool running = true;
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };
    
    struct StandbyThread {
      std::thread th;
      u64 cpuid;
      
      // oneapi::tbb::concurrent_priority_queue<std::function<void()>> jobs;
      
      bool running = true;
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };
    // -------------------------------------------------------------------------------------
    std::unordered_map<CPUID, NodeCoreSweeperThread> glb_ncore_sweeper_thrds; 
    std::unordered_map<CPUID, SysSweeperThread> glb_sys_sweeper_thrds; 
    std::unordered_map<CPUID, WorkerThread> glb_worker_thrds; 
    std::unordered_map<CPUID, MegaMindThread> glb_megamind_thrds; 
    std::unordered_map<CPUID, RouterThread> glb_router_thrds; 
    std::unordered_map<CPUID, StandbyThread> glb_standby_thrds; 

    std::unordered_map<CPUID, WorkerThread> testWkload_glb_worker_thrds; 

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
    TPManager();
    TPManager(std::vector<CPUID> ncore_sweeper_cpuids, std::vector<CPUID> sys_sweeper_cpuids, std::vector<CPUID> megamind_cpuids, std::vector<CPUID> worker_cpuids, std::vector<CPUID> router_cpuids, dm::GridManager *gm, scheduler::ResourceManager *rm);
    void initWorkerThreads();
    void initRouterThreads();
    void initMegaMindThreads();
    void initSysSweeperThreads();
    void initNCoreSweeperThreads();

    void dumpGridHWCounters(int tID);

    void terminateWorkerThreads();
    void terminateNCoreSweeperThreads();
    void terminateRouterThreads();
    void terminateMegaMindThreads();
    void terminateSysSweeperThreads();
    
    void detachAllThreads();

    void terminateTestWorkerThreads();

    void dumpNCoreSweeperThreads();


    void dumpTestGridHWCounters(vector<CPUID> cpuIds);
    void dumpGridWorkerThreadCounters(int tID);

    void testInterferenceInitWorkerThreads(vector<CPUID> worker_cpuids, int nWThreads);
    ~TPManager();
    // -------------------------------------------------------------------------------------
};


}  // namespace tp 
}  // namespace erebus

