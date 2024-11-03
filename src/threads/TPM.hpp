#ifndef PMOSS_THREADMANAGER_H_
#define PMOSS_THREADMANAGER_H_


#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <random>
// -------------------------------------------------------------------------------------
#include <bits/stdc++.h>
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
#include "utils/ScrambledZipfGenerator.hpp"
#include "utils/ZipfDist.hpp"
#include "ycsbc/uniform_generator.h"
#include "ycsbc/core_workload.h"

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
    
#if MACHINE == 0
    static const int CURR_NCORE_SWEEPER_THREADS = 8;
    static const int CURR_SYS_SWEEPER_THREADS = 1;
    static const int CURR_MEGAMIND_THREADS = 1;
    static const int CURR_ROUTER_THREADS = 8;
	  static const int CURR_WORKER_THREADS = 56;
#elif MACHINE == 1
    static const int CURR_NCORE_SWEEPER_THREADS = 2;
    static const int CURR_SYS_SWEEPER_THREADS = 1;
    static const int CURR_MEGAMIND_THREADS = 2;
    static const int CURR_ROUTER_THREADS = 2;
	  static const int CURR_WORKER_THREADS = 56;
#elif MACHINE == 2
    static const int CURR_NCORE_SWEEPER_THREADS = 2;
    static const int CURR_SYS_SWEEPER_THREADS = 1;
    static const int CURR_MEGAMIND_THREADS = 1;
    static const int CURR_ROUTER_THREADS = 2;
	  static const int CURR_WORKER_THREADS = 56;
#elif MACHINE == 5
    static const int CURR_NCORE_SWEEPER_THREADS = 4;
    static const int CURR_SYS_SWEEPER_THREADS = 1;
    static const int CURR_MEGAMIND_THREADS = 1;
    static const int CURR_ROUTER_THREADS = 4;
	  static const int CURR_WORKER_THREADS = 56;
#elif MACHINE == 6
    static const int CURR_NCORE_SWEEPER_THREADS = 4;
    static const int CURR_SYS_SWEEPER_THREADS = 1;
    static const int CURR_MEGAMIND_THREADS = 1;
    static const int CURR_ROUTER_THREADS = 4;
	  static const int CURR_WORKER_THREADS = 28;
#else
	  static const int CURR_WORKER_THREADS = 56;
#endif
    // -------------------------------------------------------------------------------------    
    static const u64 PERF_STAT_COLLECTION_INTERVAL = 100; // granularity of profiling 
    // -------------------------------------------------------------------------------------
    
    struct SysSweeperThread {
      std::thread th;
      u64 cpuid;
      oneapi::tbb::concurrent_queue<IntelPCMCounter> pcmCounters;
      bool running = true;
      bool job_set = false;   
      bool job_done = false; 
    };

    struct NodeCoreSweeperThread {
      std::thread th;
      u64 cpuid;
      vector <DataDistSnap> dataDistReel;
      vector<IntelPCMCounter> DRAMResUsageReel;
      vector<QueryExecSnap> queryExecReel; 
      bool running = true;
      bool job_set = false;   
      bool job_done = false;  
    };

    
    struct MegaMindThread {
      std::thread th;
      u64 cpuid;
      bool running = true;
      bool job_set = false;  
      bool job_done = false;  
    };
    
    struct WorkerThread {
      std::thread th;
      u64 cpuid;
      oneapi::tbb::concurrent_priority_queue<Rectangle, Rectangle::compare_f> jobs;
      oneapi::tbb::concurrent_queue<PerfCounter> perf_stats;  
      std::unordered_map<CPUID, u64> qExecutedMice;  
      std::unordered_map<CPUID, u64> qExecutedElephant;
      std::unordered_map<u64, u64> qExecutedMammoth;
      bool running = true;
      bool job_set = false;   
      bool job_done = false;
    };

    struct RouterThread {
      std::thread th;
      u64 cpuid;
      int qCorrMatrix[MAX_GRID_CELL][MAX_GRID_CELL] = {0};  
      bool running = true;
      bool job_set = false;   
      bool job_done = false;
    };
    
    struct StandbyThread {
      std::thread th;
      u64 cpuid;      
      bool running = true;
      bool job_set = false;  
      bool job_done = false; 
    };
    
    // -------------------------------------------------------------------------------------
    std::unordered_map<CPUID, NodeCoreSweeperThread> glb_ncore_sweeper_thrds; 
    std::unordered_map<CPUID, SysSweeperThread> glb_sys_sweeper_thrds; 
    std::unordered_map<CPUID, WorkerThread> glb_worker_thrds; 
    std::unordered_map<CPUID, MegaMindThread> glb_megamind_thrds; 
    std::unordered_map<CPUID, RouterThread> glb_router_thrds; 
    std::unordered_map<CPUID, StandbyThread> glb_standby_thrds; 

    std::unordered_map<CPUID, WorkerThread> testWkload_glb_worker_thrds; 
    // -------------------------------------------------------------------------------------
    TPManager();
    TPManager(std::vector<CPUID> ncore_sweeper_cpuids, std::vector<CPUID> sys_sweeper_cpuids, std::vector<CPUID> megamind_cpuids, std::vector<CPUID> worker_cpuids, std::vector<CPUID> router_cpuids, dm::GridManager *gm, scheduler::ResourceManager *rm);
    void init_worker_threads();
    void init_router_threads(int ds, int wl, double min_x, double max_x, double min_y, double max_y, std::vector<keytype> &init_keys, std::vector<uint64_t> &values);
    void init_megamind_threads();
    void init_syssweeper_threads();
    void init_ncoresweeper_threads();

    void dumpGridHWCounters(int tID);

    void terminate_worker_threads();
    void terminate_ncoresweeper_threads();
    void terminate_router_threads();
    void terminate_megamind_threads();
    void terminate_syssweeper_threads();
    
    void detachAllThreads();

    void terminateTestWorkerThreads();

    void dump_ncoresweeper_threads();


    void dumpTestGridHWCounters(vector<CPUID> cpuIds);

    void testInterferenceInitWorkerThreads(vector<CPUID> worker_cpuids, int nWThreads);
    ~TPManager();
    // -------------------------------------------------------------------------------------
};


}  // namespace tp 
}  // namespace erebus

#endif 