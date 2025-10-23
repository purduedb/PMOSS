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
	  static const int CURR_WORKER_THREADS = 78;
#elif MACHINE == 1
    static const int CURR_NCORE_SWEEPER_THREADS = 2;
    static const int CURR_SYS_SWEEPER_THREADS = 1;
    static const int CURR_MEGAMIND_THREADS = 2;
    static const int CURR_ROUTER_THREADS = 2;
	  static const int CURR_WORKER_THREADS = 56;
#elif MACHINE == 2
    static const int CURR_NCORE_SWEEPER_THREADS = 2;
    static const int CURR_SYS_SWEEPER_THREADS = 1;
    static const int CURR_MEGAMIND_THREADS = 1;   //For ycsb-insert realted set it to 1
    static const int CURR_ROUTER_THREADS = 2;
	  static const int CURR_WORKER_THREADS = 58;
#elif MACHINE == 3
    static const int CURR_NCORE_SWEEPER_THREADS = 8;
    static const int CURR_SYS_SWEEPER_THREADS = 1;
    static const int CURR_MEGAMIND_THREADS = 1;
    static const int CURR_ROUTER_THREADS = 8;
	  static const int CURR_WORKER_THREADS = 46;
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
	  static const int CURR_WORKER_THREADS = 38;
#else
	  static const int CURR_WORKER_THREADS = 56;
#endif
    // -------------------------------------------------------------------------------------    
    static const u64 PERF_STAT_COLLECTION_INTERVAL = 100; // granularity of profiling 
    // -------------------------------------------------------------------------------------
    // Query rate control configuration
    static const bool RATE_CONTROL_ENABLED = true;  // Enable/disable query rate limiting
    static const int QUERIES_PER_SECOND = 1000;    // Target queries per second per router thread
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
      // Pause/resume support for dynamic reconfiguration
      std::mutex pause_mutex;
      std::condition_variable pause_cv;
      bool paused = false;
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
      // Pause/resume support for dynamic reconfiguration
      std::mutex pause_mutex;
      std::condition_variable pause_cv;
      bool paused = false;
    };

    struct RouterThread {
      std::thread th;
      u64 cpuid;
      int qCorrMatrix[MAX_GRID_CELL][MAX_GRID_CELL] = {0};
      bool running = true;
      bool job_set = false;
      bool job_done = false;
      // Workload change synchronization
      std::atomic<int> current_workload{-1};
      std::atomic<bool> workload_change_pending{false};
      // Grid cell update synchronization (pause/resume)
      std::mutex pause_mutex;
      std::condition_variable pause_cv;
      bool paused = false;
      // Query rate control tracking
      std::chrono::steady_clock::time_point second_start_time;
      int queries_this_second = 0;
    };
    
    struct StandbyThread {
      std::thread th;
      u64 cpuid;      
      bool running = true;
      bool job_set = false;  
      bool job_done = false; 
    };
    
    struct InferenceRequest {
      int required_config;
      int required_workload;
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
    // Dynamic reconfiguration synchronization primitives
    // -------------------------------------------------------------------------------------
    // Router workload change barrier synchronization
    std::atomic<int> active_workload{-1};
    std::atomic<int> router_ready_count{0};
    std::mutex workload_change_mutex;
    std::condition_variable workload_change_cv;

    // Persistent sample counter for continuous numbering across dumps
    int dump_sample_counter = 0;
    // -------------------------------------------------------------------------------------
    TPManager();
    TPManager(std::vector<CPUID> ncore_sweeper_cpuids, std::vector<CPUID> sys_sweeper_cpuids, std::vector<CPUID> megamind_cpuids, std::vector<CPUID> worker_cpuids, std::vector<CPUID> router_cpuids, dm::GridManager *gm, scheduler::ResourceManager *rm);
    void init_worker_threads();
    void init_router_threads(int ds, int wl, double min_x, double max_x, double min_y, double max_y, std::vector<keytype> &init_keys, std::vector<uint64_t> &values);
    void init_megamind_threads(int next_config, int next_workload, std::string next_config_path, int round);
    void init_syssweeper_threads();
    void init_ncoresweeper_threads();

    void dumpGridHWCounters(int tID);

    void terminate_worker_threads();
    void terminate_ncoresweeper_threads();
    void terminate_router_threads();
    void terminate_megamind_threads();
    void terminate_syssweeper_threads();

    void detachAllThreads();

    // -------------------------------------------------------------------------------------
    // Dynamic reconfiguration methods
    // -------------------------------------------------------------------------------------
    // Worker thread pause/resume control
    void pause_all_workers();
    void resume_all_workers();
    bool are_all_workers_idle();
    void clear_all_worker_queues();  // Clear all pending queries from worker queues

    // Router thread workload synchronization
    void initiate_workload_change(int new_workload);
    void wait_for_router_sync(int router_id, int new_workload);

    // Router thread pause/resume control (for grid cell updates)
    void pause_all_routers();
    void resume_all_routers();

    // NodeCoreSweeper thread pause/resume control (for reconfiguration)
    void pause_all_ncoresweepers();
    void resume_all_ncoresweepers();

    // Parallel migration using paused worker threads
    void assign_migration_tasks(int total_cells);
    void wait_for_migration_completion();

    void terminateTestWorkerThreads();

    void dump_ncoresweeper_threads(int round);
    void dump_ncoresweeper_threads_v2();


    void dumpTestGridHWCounters(vector<CPUID> cpuIds);

    void testInterferenceInitWorkerThreads(vector<CPUID> worker_cpuids, int nWThreads);
    static bool query_gilbreth_server(const InferenceRequest& request);
    
    ~TPManager();
    // -------------------------------------------------------------------------------------
};


}  // namespace tp 
}  // namespace erebus

#endif 