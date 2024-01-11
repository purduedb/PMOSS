#include "TPM.hpp"
// -------------------------------------------------------------------------------------
#include <random>
// -------------------------------------------------------------------------------------
namespace erebus
{
namespace tp
{

TPManager::TPManager(std::vector<CPUID> megamind_cpuids, std::vector<CPUID> worker_cpuids, std::vector<CPUID> router_cpuids, dm::GridManager *gm, scheduler::ResourceManager *rm)
{
    this->gm = gm;
    this->rm = rm;

    std::mutex iomutex;

    // -------------------------------------------------------------------------------------
    // initialize worker_threads
    s16 tid = 0;
    for (unsigned i = 0; i < CURR_WORKER_THREADS; ++i) {
        // glb_worker_thrds.push_back(std::thread([&iomutex, i, this, gm, worker_cpuids]{
        glb_worker_thrds[worker_cpuids[i]].th = std::thread([&iomutex, i, this, gm, worker_cpuids]{
            erebus::utils::PinThisThread(worker_cpuids[i]);
            glb_worker_thrds[worker_cpuids[i]].cpuid=worker_cpuids[i];
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            PerfEvent e;
            
            static int cnt = 0;        
            
            while (1) {
                if (cnt % PERF_STAT_COLLECTION_INTERVAL == 0) {
                    e.startCounters();
                }
                    
                Rectangle rec_pop;
                glb_worker_thrds[worker_cpuids[i]].jobs.try_pop(rec_pop);
                int result = QueryRectangle(this->gm->idx, rec_pop.left_, rec_pop.right_, rec_pop.bottom_, rec_pop.top_);
                cout << "Threads= " << i << " Result = " << result << endl;
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
 
                if (cnt % PERF_STAT_COLLECTION_INTERVAL == 9){
                    e.stopCounters();
                    PerfCounter perf_counter;
                    for(auto j=0; j < e.events.size(); j++)
					    perf_counter.raw_counter_values[j] = e.events[j].readCounter();
				    perf_counter.normalizationConstant = 1;

                    glb_worker_thrds[worker_cpuids[i]].perf_stats.push_back(perf_counter);

                    // std::cout << "Thread =" << i << endl;
                    // e.printReport(std::cout, 10); // use n as scale factor
                    // std::cout << std::endl;
                }
                    
                cnt +=1;

            }
        });

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
    
    // -------------------------------------------------------------------------------------
    // initialize router threads
    for (unsigned i = 0; i < CURR_ROUTER_THREADS; ++i) {
        // glb_router_thrds.push_back(std::thread([&iomutex, tid, this, router_cpuids, i] {
        glb_router_thrds[router_cpuids[i]].th = std::thread([&iomutex, tid, this, router_cpuids, i, gm] {
            erebus::utils::PinThisThread(router_cpuids[i]);
            glb_router_thrds[router_cpuids[i]].cpuid=router_cpuids[i];
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

                // Check which grid it belongs to 
                std::vector<int> valid_gcells;
                for (auto gc = 0; gc < gm->nGridCells; gc++){
                    double glx = gm->glbGridCell[gc].lx;
                    double gly = gm->glbGridCell[gc].ly;
                    double ghx = gm->glbGridCell[gc].hx;
                    double ghy = gm->glbGridCell[gc].hy;

                    if (hx < glx || lx > ghx || hy < gly || ly > ghy)
                        continue;
                    else valid_gcells.push_back(gc);
                }
                std::uniform_int_distribution<int> dq(0, valid_gcells.size()-1); 
                int insert_tid = dq(gen);
                int cpuid = gm->glbGridCell[valid_gcells[insert_tid]].idCPU;
                cout << glb_worker_thrds[cpuid].cpuid << "  " << glb_worker_thrds[cpuid].th.get_id() << endl;
                glb_worker_thrds[cpuid].jobs.push(query);
                // worker_threads_meta[gm->glbGridCell[valid_gcells[insert_tid]]].jobs.push(query);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                // std::uniform_int_distribution<int> dq(0, CURR_WORKER_THREADS); 
                // int insert_tid = dq(gen);
                // worker_threads_meta[insert_tid].jobs.push(query);
                // std::this_thread::sleep_for(std::chrono::milliseconds(10));

            }
        });

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
    // -------------------------------------------------------------------------------------
    // initialize megamind threads
    for (unsigned i = 0; i < CURR_MEGAMIND_THREADS; ++i) {
        // glb_router_thrds.push_back(std::thread([&iomutex, tid, this, megamind_cpuids, i] {
        glb_router_thrds[megamind_cpuids[i]].th = std::thread([&iomutex, tid, this, megamind_cpuids, i] {
            erebus::utils::PinThisThread(megamind_cpuids[i]);
            megamind_threads_meta[i].cpuid=megamind_cpuids[i];
            while (1) 
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

            }
        });
        tid += 1;
    }
}

}  //namespace tp

} // namespace erebus
