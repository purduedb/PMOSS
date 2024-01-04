/**
 * @file threadpool.hpp
 * @author yr (Dec17,23)
 * @brief 
 * 
 */

/**
 * TODO: (a) create n(=num_cores) threads, bind it to cores,
 * (b) Have something to point to the active task sets that a thread needs to execute
 * [Probably a grid data structure where all the queries that fall in a cell are queued]
 * 
 */

#pragma once
#include "shared-headers/Units.hpp"
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "oneapi/tbb/concurrent_priority_queue.h"
#include "storage/rtree/rtree.h"
#include "shared-headers/PerfEvent.hpp"
using namespace erebus::storage::rtree;
namespace erebus
{
namespace tp
{
    
class ThreadPool{
  public:
    static const int MAX_MEGAMIND_THREADS = 10;
    static const int MAX_WORKER_THREADS = 10;
    static const int MAX_ROUTER_THREADS = 4;

    static const int CURR_MEGAMIND_THREADS = 2;
    static const int CURR_WORKER_THREADS = 4;
    static const int CURR_ROUTER_THREADS = 2;
    
    std::vector<std::thread> glb_worker_thrds; 
    std::vector<std::thread> glb_megamind_thrds;
    std::vector<std::thread> glb_router_thrds; 
    
    
    struct MegaMindThread {
      std::mutex mutex;
      std::condition_variable cv;
      // oneapi::tbb::concurrent_priority_queue<std::function<void()>> jobs;
      
      bool wt_ready = true;   // Idle
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };
    
    struct WorkerThread {
      std::mutex mutex;
      std::condition_variable cv;
      oneapi::tbb::concurrent_priority_queue<Rectangle, Rectangle::compare_f> jobs;
      bool wt_ready = true;   // Idle
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };

    struct RouterThread {
      std::mutex mutex;
      std::condition_variable cv;
      // oneapi::tbb::concurrent_priority_queue<std::function<void()>> jobs;
      
      bool wt_ready = true;   // Idle
      bool job_set = false;   // Has job
      bool job_done = false;  // Job done
    };
   
   MegaMindThread megamind_threads_meta[MAX_MEGAMIND_THREADS];
   WorkerThread worker_threads_meta[MAX_WORKER_THREADS];
   RouterThread router_threads_meta[MAX_ROUTER_THREADS];
   
   ThreadPool(std::vector<int> megamind_cpuids, std::vector<int> worker_cpuids, std::vector<int> router_cpuids, RTree *rtree);
};


}  // namespace tp 

}  // namespace erebus

