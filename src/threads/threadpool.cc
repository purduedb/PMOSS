/*
* TODO: (a) create n(=num_cores) threads, bind it to cores,
* (b) Have something to point to the active task sets that a thread needs to execute
* [Probably a grid data structure where all the queries that fall in a cell are queued]
* 
*/
#pragma once

// TODO: Create n number of megamind threads that can handle incoming queries be it range or any other
// thingy


#include "threadpool.hpp"
#include <random>
#include "utils/Misc.hpp"

namespace erebus{
namespace tp{

ThreadPool::ThreadPool(std::vector<int> megamind_cpuids, std::vector<int> worker_cpuids, std::vector<int> router_cpuids, RTree *rtree){
    // unsigned num_cpus = std::thread::hardware_concurrency();
    // std::cout << "Launching " << num_cpus << " threads\n";
    
    // A mutex ensures orderly access to std::cout from multiple threads.
    std::mutex iomutex;
    
    
    // initialize worker_threads
    s16 tid = 0;
    for (unsigned i = 0; i < CURR_WORKER_THREADS; ++i) {
        glb_worker_thrds.push_back(std::thread([&iomutex, i, this, rtree, worker_cpuids] {
            
            erebus::utils::PinThisThread(worker_cpuids[i]);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            PerfEvent e;
            
            static int cnt = 0;        
            
            while (1) {
                if (cnt%100==0) 
                    e.startCounters();    
                
                Rectangle rec_pop;
                worker_threads_meta[i].jobs.try_pop(rec_pop);
                int result = QueryRectangle(rtree, rec_pop.left_, rec_pop.right_, rec_pop.bottom_, rec_pop.top_);
                cout << "Threads= " << i << " Result = " << result << endl;
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                // {
                // // Use a lexical scope and lock_guard to safely lock the mutex only
                // // for the duration of std::cout usage.
                // std::lock_guard<std::mutex> iolock(iomutex);
                // std::cout << "Thread #" << i << ": on CPU " << sched_getcpu() << "\n";
                // }

                // Simulate important work done by the tread by sleeping for a bit...
                // std::this_thread::sleep_for(std::chrono::milliseconds(900));
                
                if (cnt%100==9){
                    e.stopCounters();
                    std::cout << "Thread =" << i << endl;
                    e.printReport(std::cout, 10); // use n as scale factor
                    std::cout << std::endl;
                }
                    
                cnt +=1;

            }
        }));

        // Create a cpu_set_t object representing a set of CPUs. Clear it and mark
        // only CPU i as set.
        // cpu_set_t cpuset;
        // CPU_ZERO(&cpuset);
        // CPU_SET(worker_cpuids[i], &cpuset);
        // int rc = pthread_setaffinity_np(glb_worker_thrds[i].native_handle(), sizeof(cpu_set_t), &cpuset);
        // if (rc != 0) 
        //     std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
        tid += 1;
    }
    
    // initialize router threads
    for (unsigned i = 0; i < CURR_ROUTER_THREADS; ++i) {
        glb_router_thrds.push_back(std::thread([&iomutex, tid, this, router_cpuids, i] {
            
            erebus::utils::PinThisThread(router_cpuids[i]);
            
            while (1) {
                

                std::random_device rd;  // Seed the engine with a random value from the hardware
                std::mt19937 gen(rd()); // Mersenne Twister 19937 generator
                int max_pt = 100000;

                std::uniform_int_distribution<int> d1(0, int(max_pt/1000)); 
                std::uniform_int_distribution<int> d2(0, int(max_pt/1000)); 
                std::uniform_int_distribution<int> d3(0, max_pt-150); 
                std::uniform_int_distribution<int> d4(0, max_pt-150); 
                int width = d1(gen);
                int height = d2(gen);
                int lx = d3(gen);
                int ly = d4(gen);
                int hx = lx + width;
                int hy = ly + height;
                Rectangle query(lx, ly, hx, hy);

                std::uniform_int_distribution<int> dq(0, CURR_WORKER_THREADS); 
                int insert_tid = dq(gen);
                // cout << lx << " " << insert_tid << endl;
                worker_threads_meta[insert_tid].jobs.push(query);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

            }
        }));

        // Create a cpu_set_t object representing a set of CPUs. Clear it and mark
        // only CPU i as set.
        // cpu_set_t cpuset;
        // CPU_ZERO(&cpuset);
        // CPU_SET(router_cpuids[i], &cpuset);
        // int rc = pthread_setaffinity_np(glb_router_thrds[i].native_handle(), sizeof(cpu_set_t), &cpuset);
        // if (rc != 0) 
        //     std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
        tid += 1;
    }
}

}  //namespace tp

} // namespace erebus
