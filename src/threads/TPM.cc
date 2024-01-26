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
                int size_jobqueue = glb_worker_thrds[worker_cpuids[i]].jobs.size();
                if (size_jobqueue != 0){
                    glb_worker_thrds[worker_cpuids[i]].jobs.try_pop(rec_pop);
                    int result = QueryRectangle(this->gm->idx, rec_pop.left_, rec_pop.right_, rec_pop.bottom_, rec_pop.top_);
                    cout << "Threads= " << worker_cpuids[i] << " Result = " << result << endl;
                }
                
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
                // -------------------------------------------------------------------------------------
                // Generate a query 
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
                
                // -------------------------------------------------------------------------------------
                // Check which grid the query belongs to 
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

                // -------------------------------------------------------------------------------------
                /**
                 * TODO: Stamp the query whether it is a mice, elephant or mammoth
                 * I: Requires an inference model or RL model.
                 * 
                 * Things to consider: Of course you have MICE|ELEPHANT|MAMMOTH
                 * Q: What if the MICE and ELEPHANTs are accessing the same area?
                 * A: Then whoever goes first helps the other MICE|ELEPHANT.
                 * SO, they sohould be run one after another, so that they can benefit from one another. 
                 * DONOT put prioritize MICEs in such case. 
                 * TAKEAWAY: In the JOBQUEUE the spatial distance between the queries are important as well 
                 * besides the regular query classifications.
                 * 
                 * REQUIREMENT: This needs to be as lightweight as possible, why? 
                 * REQUIREMENT: Can you implement this with SIMD to make it train faster?
                 * REQUIREMENT: You want to spend as little time as possible while doing this administrative stuff before executing the queries 
                 * itself.
                 * What is interesting compared to the Onur Motlu paper: 
                 *      1. We have an index to consider.
                 *      2. We have the system
                 *      3. The definition should change considering the state of the index and the system as well.
                 *          Q: For different sized index, does the definition of the MICE, ELEPHANT and MAMMOTH change?
                 *              A: It could be counters related to CPU, Cache and Memory
                 *          Q: How to capture different sized index into features? 
                 *          Q: Can hw counters help in capturing the data distribution of the index?
                 *          Q: How to ensure this robustness?
                 *      
                 * 
                 * Features: What features do we have [before] running the query iteself?
                 *      1. QUERY SEMANTICS: SELECT, RANGE QUERY, KNN QUERY (we will think of only RANGE QUERIES OR POINT QUERIES)
                 *      2. RANGE QUERY: lx, ly, hx, hy
                 *      3. PREVIOUS HW COUNTERS OF THE DESTINATION CORE OF THIS QUERY:
                 *      4. PREVIOUS HW COUNTERS ACROSS ALL THE CORES:
                 * 
                 *      
                 * Prediction: What could we predict that will help us decide MICE|ELEPHANT|MAMMOTH?
                 *      1. CPU INSTRUCTIONS
                 *      2. CACHE ACCESSES OR MISSES (which one?)
                 * 
                 *      
                 * I donot think we care about NUMA here, only the pure values that this query will generate.
                 * 
                 * POSSIBLE MODELS: Linear Regression, Spline Regression
                 * 
                 * 
                 * O: Now the query is a mice, elephant or mammoth is relative to the work put in by the index itself.
                 * Do we go for the maximum of the system or the maximum by the index?
                 * E.g., 10 MemMissesPerKInstruction = Mice, 100 MemMissesPerKInstruction = Elephant, 1000 MemMissesPerKInstruction = Mammoth
                 * But they do not saturate the memory bw of the system, neither they affect each other 
                 * Remember the index dummy experiment I did, that should give an idea
                 * My guess is: there is a tipping point for those 
                 * 
                 * Similar Job: Predicting selectivity with lightweight models
                 * 
                 * It does not necessarily have to be hw counters; but of course that helps!
                 * 
                 * O: The best way would be: you look at the query points and you have a notion about the current status of the index (the hw parameters you are generating),
                 * based on that you can prdict the hw counters (a specific hw counter, which helps you to classify it). E.g., Memory Misses per instruction 
                 * Reference: [Onur Motlu Paper], [selectivity functions are learnable paper]
                 * 
                 * Q: Given the hw counters and the type of queries can you decide on the data distribution of the index?
                 * 
                 * O: It could be a conitnuous RL setting 
                 * 
                 * O: When do you re-train the model, when the data distribution of the index has changed. NO RL, just inference
                 * Reference: https://arxiv.org/pdf/2210.05508.pdf
                 * 
                 * O: Why not correct the model after running the query, you generate the statistics and see if the query stamp that was 
                 * stamped by the router thread is correct or not. IF not, then have a counter, given it reaches a certain threshold 
                 * it will let the router know that it needs to update itself or RL agent know that it needs to explore more?
                 * 
                 * 
                 * 
                 * I: Clustering could be another way as well which is different from query stamping, however let's not dive intot that
                 * 
                 * 
                */
                

                
                // -------------------------------------------------------------------------------------
                // Push the query to the correct worker thread's job queue
                std::uniform_int_distribution<int> dq(0, valid_gcells.size()-1); 
                int insert_tid = dq(gen);
                int cpuid = gm->glbGridCell[valid_gcells[insert_tid]].idCPU;
                // cout << glb_worker_thrds[cpuid].cpuid << "  " << glb_worker_thrds[cpuid].th.get_id() << endl;
                glb_worker_thrds[cpuid].jobs.push(query);
                // worker_threads_meta[gm->glbGridCell[valid_gcells[insert_tid]]].jobs.push(query);


                // -------------------------------------------------------------------------------------
                // Go to sleep 
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
