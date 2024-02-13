#include "TPM.hpp"
// #include "profiling/PCMMem.hpp"
// -------------------------------------------------------------------------------------

namespace erebus
{
namespace tp
{

TPManager::TPManager(std::vector<CPUID> ncore_sweeper_cpuids, std::vector<CPUID> sys_sweeper_cpuids, std::vector<CPUID> megamind_cpuids, std::vector<CPUID> worker_cpuids, std::vector<CPUID> router_cpuids, dm::GridManager *gm, scheduler::ResourceManager *rm)
{
    this->gm = gm;
    this->rm = rm;
    this->router_cpuids = router_cpuids;
    this->worker_cpuids = worker_cpuids;
    this->megamind_cpuids = megamind_cpuids;
    this->sys_sweeper_cpuids = sys_sweeper_cpuids;
    this->ncore_sweeper_cpuids = ncore_sweeper_cpuids;
}
// TPManager::~TPManager(){
    
// }
void TPManager::initWorkerThreads(){
    // -------------------------------------------------------------------------------------
    // initialize worker_threads
    for (unsigned i = 0; i < CURR_WORKER_THREADS; ++i) {
        glb_worker_thrds[worker_cpuids[i]].th = std::thread([i, this]{
            erebus::utils::PinThisThread(worker_cpuids[i]);
            glb_worker_thrds[worker_cpuids[i]].cpuid=worker_cpuids[i];
            
            PerfEvent e;
            
            while (1) {  
                if(!glb_worker_thrds[worker_cpuids[i]].running) break;
                
                Rectangle rec_pop;
                int size_jobqueue = glb_worker_thrds[worker_cpuids[i]].jobs.size();
                
                if (size_jobqueue != 0){
                    glb_worker_thrds[worker_cpuids[i]].jobs.try_pop(rec_pop);
                    e.startCounters();
                    
                    int result = QueryRectangle(this->gm->idx, rec_pop.left_, rec_pop.right_, rec_pop.bottom_, rec_pop.top_);
                    
                    
                    e.stopCounters();
                    
                    PerfCounter perf_counter;
                    for(auto j=0; j < e.events.size(); j++){
					    perf_counter.raw_counter_values[j] = e.events[j].readCounter();
                    }
				    
                    
                    perf_counter.normalizationConstant = 1;
                    perf_counter.rscan_query = rec_pop;
                    perf_counter.result = result;
                    perf_counter.gIdx = rec_pop.aGrid;
                    
                    // You push this info to all the grids this incomoig query intersect
                    // for(auto qlog = 0; qlog < rec_pop.validGridIds.size(); qlog++){
                    //     int gridId = rec_pop.validGridIds[qlog];
                    //     glb_worker_thrds[worker_cpuids[i]].shadowDataDist[gridId].perf_stats.push_back(perf_counter);
                    // }
                    
                    
                    // glb_worker_thrds[worker_cpuids[i]].shadowDataDist[rec_pop.aGrid].perf_stats.push_back(perf_counter);
                    glb_worker_thrds[worker_cpuids[i]].perf_stats.push(perf_counter);

                    /**
                     * TODO: You should be updating the outstanding queries 
                    */
                    gm->freqQueryDistCompleted[rec_pop.aGrid] += 1;
                    
                    // cout << "Threads= " << worker_cpuids[i] << " Result = " << result << " " << rec_pop.validGridIds[0] << endl;
                    // std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                
            }
        });
    }
}


void TPManager::initRouterThreads(){
    // -------------------------------------------------------------------------------------
    // initialize router threads
    for (unsigned i = 0; i < CURR_ROUTER_THREADS; ++i) {
        glb_router_thrds[router_cpuids[i]].th = std::thread([i, this] {
            erebus::utils::PinThisThread(router_cpuids[i]);
            glb_router_thrds[router_cpuids[i]].cpuid=router_cpuids[i];
            
            while (1) {
                // -------------------------------------------------------------------------------------
                // Generate a query 
                std::random_device rd;  // Seed the engine with a random value from the hardware
                std::mt19937 gen(rd()); // Mersenne Twister 19937 generator
                double min_x, max_x, min_y, max_y;
                min_x = -83.478714;
                max_x = -65.87531;
                min_y = 38.78981;
                max_y = 47.491634;
                
                double max_length = 8;
                double max_width = 8;

    #if DIST_UNIFORM
                std::uniform_real_distribution<> dlx(min_x, max_x);
                std::uniform_real_distribution<> dly(min_y, max_y);
                double lx = dlx(gen);
                double ly = dly(gen);

                std::uniform_real_distribution<> dLength(0, max_length);
                std::uniform_real_distribution<> dWidth(0, max_width);
                double length = dLength(gen);
                double width = dWidth(gen);
                
                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);
                
    #else
                double avg_x = (max_x + min_x) / 2;
                double avg_y = (max_y + min_y)/ 2;
                
                double dev_x = 3;
                double dev_y = 3;

                std::normal_distribution<double> dlx(avg_x, dev_x);
                std::normal_distribution<double> dly(avg_y, dev_y);

                double lx = dlx(gen);
                double ly = dly(gen);

                max_length = 3;
                max_width = 3;

                std::uniform_real_distribution<> dLength(1, max_length);
                std::uniform_real_distribution<> dWidth(1, max_width);
                double length = dLength(gen);
                double width = dWidth(gen);
                
                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);

    #endif

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
                    else {
                        /**
                         * 1. Store IDs of the Grids that the query intersects
                         * 2. Update the query frequency
                         * 3. Update the query's valid grid cells so that it can maintain a local view of the data distribution
                        */
                        valid_gcells.push_back(gc);  
                        // gm->freqQueryDistPushed[gc]++;  // I am currently only keeping where it goes, don't care about  the intersections
                        query.validGridIds.push_back(gc);
                    }        
                }
                
                // -------------------------------------------------------------------------------------
                // Check the sanity of the query
                if (valid_gcells.size() == 0) continue;
                // -------------------------------------------------------------------------------------
                // TODO: Update the Query Correlation Matrix
                for(size_t qc1 = 0; qc1 < valid_gcells.size()-1; qc1++){
                    int pCell = valid_gcells[qc1];
                    for(size_t qc2 = qc1; qc2 < valid_gcells.size(); qc2++){
                        int cCell = valid_gcells[qc2];
                        this->gm->qCorrMatrix[pCell][cCell] ++;
                        this->gm->qCorrMatrix[cCell][pCell] ++;
                    }
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
                
                // cout << valid_gcells.size() << " " << endl;
                
                // Push the query to the correct worker thread's job queue
                std::mt19937 genInt(rd());
                std::uniform_int_distribution<int> dq(0, valid_gcells.size()-1); 
                int insert_tid = dq(genInt);

                int glbGridCellInsert = valid_gcells[insert_tid];
                
                // -------------------------------------------------------------------------------------
                // Stamping the query with something: You need to do the inverse of log_2
                // For now skipping
            #if USE_MODEL
                double predictIns = query.left_ * gm->glbGridCell[glbGridCellInsert].lRegCoeff[0][0] + 
                                    query.right_ * gm->glbGridCell[glbGridCellInsert].lRegCoeff[0][1]+ 
                                    query.bottom_ * gm->glbGridCell[glbGridCellInsert].lRegCoeff[0][2]+ 
                                    query.top_ * gm->glbGridCell[glbGridCellInsert].lRegCoeff[0][3];
                
                double predictAcc = query.left_ * gm->glbGridCell[glbGridCellInsert].lRegCoeff[1][0] + 
                                    query.right_ * gm->glbGridCell[glbGridCellInsert].lRegCoeff[1][1]+ 
                                    query.bottom_ * gm->glbGridCell[glbGridCellInsert].lRegCoeff[1][2]+ 
                                    query.top_ * gm->glbGridCell[glbGridCellInsert].lRegCoeff[1][3];
                
                
                if (predictIns < QUERY_THRESHOLD_INS && predictAcc < QUERY_THRESHOLD_ACC) 
                    query.qStamp = QUERY_MICE;
                else if (predictIns >= QUERY_THRESHOLD_INS && predictAcc >= QUERY_THRESHOLD_ACC)
                    query.qStamp = QUERY_MAMMOTH;
                else 
                    query.qStamp = QUERY_ELEPHANT;
            #endif

                // -------------------------------------------------------------------------------------
                query.aGrid = glbGridCellInsert;
                // -------------------------------------------------------------------------------------
                // Update the query view of each cell
                gm->glbGridCell[glbGridCellInsert].qType[query.qStamp] += 1;
                gm->freqQueryDistPushed[glbGridCellInsert]++;
                gm->freqQueryDistCompleted[glbGridCellInsert]++;
                // -------------------------------------------------------------------------------------
                /**
                 * TODO: The idCpu can be a vector, as multiple threads might be allocated to this grid 
                */
                int cpuid = gm->glbGridCell[glbGridCellInsert].idCPU;
                glb_worker_thrds[cpuid].jobs.push(query);
                // -------------------------------------------------------------------------------------
                // std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
}


void TPManager::initMegaMindThreads(){
    // -------------------------------------------------------------------------------------
    for (unsigned i = 0; i < CURR_MEGAMIND_THREADS; ++i) {
        glb_megamind_thrds[megamind_cpuids[i]].th = std::thread([i, this] {
            erebus::utils::PinThisThread(megamind_cpuids[i]);
            glb_megamind_thrds[megamind_cpuids[i]].cpuid=megamind_cpuids[i];
            int numaID = numa_node_of_cpu(megamind_cpuids[i]);
            while (1) 
            {
                // std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                // PerfCounter perf_counter;
                // perf_counter.qType = SYNC_TOKEN;
                // for (auto[itr, rangeEnd] = this->gm->NUMAToWorkerCPUs.equal_range(numaID); itr != rangeEnd; ++itr)
                // {
                //     int wkCPUID = itr->second;
                //     // cout << itr->first<< '\t' << itr->second << '\n';
                //     glb_worker_thrds[wkCPUID].perf_stats.push(perf_counter);
                // }

                // IntelPCMCounter iPCMCnt;
                // iPCMCnt.qType = SYNC_TOKEN;
                // glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.push(iPCMCnt);
                

            }
        });
    }
}


void TPManager::initSysSweeperThreads(){
    // -------------------------------------------------------------------------------------
    // initialize megamind threads
    for (unsigned i = 0; i < CURR_SYS_SWEEPER_THREADS; ++i) {
        glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].th = std::thread([i, this] {
            erebus::utils::PinThisThread(sys_sweeper_cpuids[i]);
            glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].cpuid=sys_sweeper_cpuids[i];
            
            // -------------------------------------------------------------------------------------
            // Params for DRAM Throughput
            double delay = 30000;
            bool csv = false, csvheader = false, show_channel_output = true, print_update = false;
            uint32 no_columns = DEFAULT_DISPLAY_COLUMNS; // Default number of columns is 2
            ServerUncoreMemoryMetrics metrics = PartialWrites;
            
            // -------------------------------------------------------------------------------------
            // Params for UPI links
            std::vector<CoreCounterState> cstates1, cstates2;
            std::vector<SocketCounterState> sktstate1, sktstate2;
            SystemCounterState sstate1, sstate2;
            // -------------------------------------------------------------------------------------
            
            
            PCM * m = PCM::getInstance();
            PCM::ErrorCode returnResult = m->program();

            if (returnResult != pcm::PCM::Success){
                	std::cerr << "Intel's PCM couldn't start" << std::endl;
                	std::cerr << "Error code: " << returnResult << std::endl;
                	exit(1);
            }
            uint32 imc_channels = (pcm::uint32)m->getMCChannelsPerSocket();
            uint32 numSockets = m->getNumSockets();
            const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();

            // -------------------------------------------------------------------------------------
            // Params for DRAM Throughput
            int rankA = -1, rankB = -1;
            uint64 SPR_CHA_CXL_Event_Count = 0;
            rankA = 0;
            rankB = 1;
            std::vector<ServerUncoreCounterState> BeforeState(m->getNumSockets());
            std::vector<ServerUncoreCounterState> AfterState(m->getNumSockets());
            // -------------------------------------------------------------------------------------

            

            memdata_t mDataCh;
            uint64 BeforeTime = 0, AfterTime = 0;
            
            
            
            while (1) 
            {
                IntelPCMCounter iPCMCnt;

                readState(BeforeState);
                m->getAllCounterStates(sstate1, sktstate1, cstates1);
                BeforeTime = m->getTickCount();
                

                MySleepMs(delay);
                
                AfterTime = m->getTickCount();
                readState(AfterState);
                mDataCh = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns, metrics,
                        show_channel_output, print_update, SPR_CHA_CXL_Event_Count);

                m->getAllCounterStates(sstate2, sktstate2, cstates2);

                
                // TODO: Need to process these values
                for (uint32 skt = 0; skt < m->getNumSockets(); ++skt)
                {
                    // cout << " SKT   " << setw(2) << i << "     ";
                    // for (uint32 l = 0; l < qpiLinks; ++l)
                    //     cout << unit_format(getIncomingQPILinkBytes(i, l, sstate1, sstate2)) << "   ";
                    if (m->qpiUtilizationMetricsAvailable())
                    {
                        // cout << "|  ";
                        for (uint32 l = 0; l < qpiLinks; ++l){
                            iPCMCnt.UPIUtilize[skt][l]   = int(100. * getIncomingQPILinkUtilization(skt, l, sstate1, sstate2));
                            // cout << setw(3) << std::dec << int(100. * getIncomingQPILinkUtilization(i, l, sstate1, sstate2)) << "%   ";
                        }
                            
                    }
                    // cout << "\n";
                }
                    
                
                // TODO: For now skipping the ranks stuff
                // calculate_bandwidth_rank(m, BeforeState, AfterState, AfterTime - BeforeTime, csv, csvheader, 
                //     no_columns, rankA, rankB);

                // TODO: This should be inserted once you make a cycle of collectiing all the rank values
                
                iPCMCnt.sysParams = mDataCh;
                glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].pcmCounters.push(iPCMCnt);
                 
    
                
                swap(BeforeTime, AfterTime);
                swap(BeforeState, AfterState);
                std::swap(sstate1, sstate2);
                std::swap(sktstate1, sktstate2);
                std::swap(cstates1, cstates2);

                if(rankA == 6) rankA = 0;
                else rankA += 2;
                
                if(rankB == 7) rankB = 1;
                else rankB += 2;      

            }
        });
    }
}


void TPManager::initNCoreSweeperThreads(){
    for (unsigned i = 0; i < CURR_NCORE_SWEEPER_THREADS; ++i) {
        glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].th = std::thread([i, this] {
            erebus::utils::PinThisThread(ncore_sweeper_cpuids[i]);
            glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].cpuid=ncore_sweeper_cpuids[i];
            int numaID = numa_node_of_cpu(ncore_sweeper_cpuids[i]);
            while (1) 
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(80000));
                // -------------------------------------------------------------------------------------
                // First push the token 
                PerfCounter perf_counter;
                perf_counter.qType = SYNC_TOKEN;
                for (auto[itr, rangeEnd] = this->gm->NUMAToWorkerCPUs.equal_range(numaID); itr != rangeEnd; ++itr)
                {
                    int wkCPUID = itr->second;
                    // cout << itr->first<< '\t' << itr->second << '\n';
                    glb_worker_thrds[wkCPUID].perf_stats.push(perf_counter);
                }

                IntelPCMCounter iPCMCnt;
                iPCMCnt.qType = SYNC_TOKEN;
                glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.push(iPCMCnt);
                // -------------------------------------------------------------------------------------
                // Then start reading

                const int nQCounterCline = PERF_EVENT_CNT/8 + 1;
                
                
                /**
                 * It has to be a complete snap.
                 * Unless for all the cores you have got the token
                 * do not insert
                */
                DataDistSnap ddSnap;  // Snapshot for the current NUMA node
                
                for (auto[itr, rangeEnd] = this->gm->NUMAToWorkerCPUs.equal_range(numaID); itr != rangeEnd; ++itr)
                {
                    int wkCPUID = itr->second;
                    bool token_found = false;                    
                    while(!token_found){
                        size_t size_stats = glb_worker_thrds[wkCPUID].perf_stats.unsafe_size();
                        PerfCounter pc;
                        if (size_stats != 0){
                            
                            glb_worker_thrds[wkCPUID].perf_stats.try_pop(pc);
                            if (pc.qType == SYNC_TOKEN){
                                break;
                            }
                    
                            // Use SIMD to compute the DataView
                            ddSnap.rawCntSamples[pc.gIdx] += 1; 
                            __m512d rawQCounter[nQCounterCline];
                            __m512d nIns= _mm512_set1_pd (pc.raw_counter_values[1]);
                            for (auto vCline = 0; vCline < nQCounterCline; vCline++){
                                rawQCounter[vCline] = _mm512_load_pd (pc.raw_counter_values + vCline * 8);
                                rawQCounter[vCline] = _mm512_div_pd (rawQCounter[vCline], nIns);
                                rawQCounter[vCline] = _mm512_mul_pd (rawQCounter[vCline], _mm512_set1_pd (1000));
                                ddSnap.rawQCounter[pc.gIdx][vCline]  = _mm512_add_pd (ddSnap.rawQCounter[pc.gIdx][vCline], rawQCounter[vCline]);
                            }                            
   
                        }
                        else
                            break;
                    }
                    
                }
                glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].dataDistReel.push_back(ddSnap);
                // Calculate the Average
                /**
                 * TODO: This does not tell you the full story: there can be a huge outlier
                 * which can mess up the whole average
                 * Hence the stat function to use here is just absurd I think since
                 * we are doing the dirty work with the data anyway:
                 * Need std_deviation, or just a probabilistic view
                 * */ 
                
            }
        });
    }
}



void TPManager::dumpGridHWCounters(int tID){
    bool isFeaturized[MAX_GRID_CELL] = {0};
    
    for(auto gIdx = 0; gIdx < MAX_GRID_CELL; gIdx++){
        // if (isFeaturized[gIdx] == false) {
            
        //     continue;
        // }
        // -------------------------------------------------------------------------------------
        ofstream input_qstamp("/homes/yrayhan/works/erebus/src/qstamp_features/g" + std::to_string(gIdx) + ".txt", std::ifstream::out);
        // -------------------------------------------------------------------------------------
        // Find the appropriate thread
        int cpuid = this->gm->glbGridCell[gIdx].idCPU;
        // -------------------------------------------------------------------------------------
        // Find the appropriate thread and gIdx 
        // Cleaner version should do this more optimally since each thread can actually 
        // store at multiple gridIdxs
        int sHist = this->glb_worker_thrds[cpuid].shadowDataDist[gIdx].perf_stats.size();
        for(auto j = 0; j < sHist; j++){
            PerfCounter hist = this->glb_worker_thrds[cpuid].shadowDataDist[gIdx].perf_stats[j];
            input_qstamp << hist.rscan_query.left_ << " "
            << hist.rscan_query.right_ << " "
            << hist.rscan_query.bottom_ << " "
            << hist.rscan_query.top_ << " ";
            for (auto k = 0; k < PERF_EVENT_CNT; k++){
                input_qstamp << hist.raw_counter_values[k] << " ";
            }
            input_qstamp << hist.result;
            input_qstamp << endl;
        }
        if (gIdx % 100 == 0) cout << gIdx << "++++++++++" << endl;
        
    }
    
}

void TPManager::dumpGridWorkerThreadCounters(int tID){
    cout << "==========================DUMPING STUFF=======================" << endl;
    for (const auto & [ key, value ] : glb_worker_thrds) {
        ofstream input_qstamp("/homes/yrayhan/works/erebus/src/qstamp_features_th/t" + std::to_string(key) + ".csv", std::ifstream::app);
        
        for (auto g = 0; g < 1000; g++){
            int sHist = glb_worker_thrds[key].shadowDataDist[g].perf_stats.size();
            if (sHist == 0) continue;
            for(auto j = 0; j < sHist; j++){
                PerfCounter hist = glb_worker_thrds[key].shadowDataDist[g].perf_stats[j];
                input_qstamp << key << " "
                 << g << " "   
                 << hist.rscan_query.left_ << " "
                 << hist.rscan_query.right_ << " "
                 << hist.rscan_query.bottom_ << " "
                 << hist.rscan_query.top_ << " ";
                for (auto k = 0; k < PERF_EVENT_CNT; k++){
                    input_qstamp << hist.raw_counter_values[k] << " ";
                }
                input_qstamp << hist.result;
                input_qstamp << endl;
            }
        }   
        cout << "==========================Thread" << key << "=======================" << endl;
    }    
    cout << "==================================================================" << endl;
}


void TPManager::terminateWorkerThreads(){
    for (const auto & [ key, value ] : glb_worker_thrds) {
        glb_worker_thrds[key].running = false;
    }
}

void TPManager::testInterferenceInitWorkerThreads(vector<CPUID> test_worker_cpuids, int nWThreads){
    // -------------------------------------------------------------------------------------
    // https://stackoverflow.com/questions/1577475/c-sorting-and-keeping-track-of-indexes
    vector<int> V(100);
    std::iota(V.begin(), V.end(), 0); //Initializing
    sort( V.begin(),V.end(), [&](int i,int j){return this->gm->DataDist[i] > this->gm->DataDist[j];} );
    for (auto i = 0; i < V.size(); i++)
        cout << "[ " << V[i] << " " << this->gm->DataDist[V[i]] << " ] ";
    cout << endl;
    /**
     * 0: [393 - 1613]
     * 1: [11564 - 40018]
     * 2: [116317 - 939387]
    */
    
    // -------------------------------------------------------------------------------------
    for (int i = 0; i < nWThreads; ++i) {
        
        testWkload_glb_worker_thrds[test_worker_cpuids[i]].th = std::thread([i, this, 
            test_worker_cpuids, nWThreads] {
            // double gIdxs[3][10] = {
            //     {51, 45, 36, 21, 12, 35, 44, 57, 22, 62},
            //     {72, 32, 50, 78, 55, 13, 65, 40, 56, 97},
            //     {64, 73, 63, 54, 74, 75, 76, 52, 77, 43}
            // };
            double gIdxs[2][16] = {
                {97, 47, 96, 3, 85, 36, 79, 20, 30, 2, 14, 98, 10, 4, 95, 91},
                {74, 64, 73, 52, 41, 63, 32, 53, 42, 21, 31, 54, 22, 65, 51, 66}
                
            };
            
            // double gIdxs[20] = {
            //     72, 32, 50, 78, 55, 13, 65, 40, 56, 97, 
            //     64, 73, 63, 54, 74, 75, 76, 52, 77, 43
            // };
            // double gIdxs[20] = {51, 45, 36, 21, 12, 35, 44, 57, 22, 62,
            // 11, 30, 66, 2, 20, 14, 67, 4, 3, 23};
            
            double qType[nWThreads] = {
                0, 0, 0, 0, 0, 0, 0, 0, 
                0, 0, 0, 0, 0, 0, 0, 0, 
                0, 0, 0, 0, 0, 0, 0, 0
            };
            erebus::utils::PinThisThread(test_worker_cpuids[i]);
            testWkload_glb_worker_thrds[test_worker_cpuids[i]].cpuid=test_worker_cpuids[i];
            
            // -------------------------------------------------------------------------------------
            // Dataset Limits
            double min_x, max_x, min_y, max_y;
            min_x = -83.478714;
            max_x = -65.87531;
            min_y = 38.78981;
            max_y = 47.491634;

            while (1) {
                if(!testWkload_glb_worker_thrds[test_worker_cpuids[i]].running) break;
                // -------------------------------------------------------------------------------------
                std::random_device rd;
                std::mt19937 genGridIdx(rd()); // Mersenne Twister 19937 generator
                int pickGrid = -1;
                std::uniform_int_distribution<> dGrid(0, 15);  // TODO: be careful of this
                if (qType[i] == 0) pickGrid = gIdxs[0][dGrid(genGridIdx)];
                else if (qType[i] == 1) pickGrid = gIdxs[1][dGrid(genGridIdx)];
                // else if (qType[i] == 2) pickGrid = gIdxs[2][dGrid(genGridIdx)];
                
                
                // -------------------------------------------------------------------------------------
                // Generate a query 
                Rectangle query(
                    this->gm->glbGridCell[pickGrid].lx, 
                    this->gm->glbGridCell[pickGrid].hx,
                    this->gm->glbGridCell[pickGrid].ly,
                    this->gm->glbGridCell[pickGrid].hy
                    );

                // Generate a query 
                // std::random_device rd;  // Seed the engine with a random value from the hardware
                // std::mt19937 gen(rd()); // Mersenne Twister 19937 generator
                
                  
                // double max_length = 2;
                // double max_width = 2;

                // std::uniform_real_distribution<> dlx(min_x, max_x);
                // std::uniform_real_distribution<> dly(min_y, max_y);
                // double lx = dlx(gen);
                // double ly = dly(gen);

                // std::uniform_real_distribution<> dLength(0, max_length);
                // std::uniform_real_distribution<> dWidth(0, max_width);
                // double length = dLength(gen);
                // double width = dWidth(gen);
                
                // double hx = lx + length;
                // double hy = ly + width;

                // Rectangle query(lx, hx, ly, hy);
                
                // -------------------------------------------------------------------------------------
                PerfEvent e;
                e.startCounters();
                int result = QueryRectangle(this->gm->idx, query.left_, query.right_, query.bottom_, query.top_);    
                e.stopCounters();

                PerfCounter perf_counter;
                for(auto j=0; j < e.events.size(); j++)
                    perf_counter.raw_counter_values[j] = e.events[j].readCounter();
                perf_counter.normalizationConstant = 1;
                perf_counter.rscan_query = query;
                perf_counter.result = result;
                
                testWkload_glb_worker_thrds[test_worker_cpuids[i]].stats_.perf_stats.push_back(perf_counter);
                
                // -------------------------------------------------------------------------------------
                // std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
}

void TPManager::terminateTestWorkerThreads(){
    for (const auto & [ key, value ] : testWkload_glb_worker_thrds) {
        testWkload_glb_worker_thrds[key].running = false;
    }
}

void TPManager::dumpTestGridHWCounters(vector<CPUID> cpuIds){
    // -------------------------------------------------------------------------------------
    ofstream input_qstamp("/homes/yrayhan/works/erebus/src/interference/new/mice24_log.txt", std::ifstream::app);
    // -------------------------------------------------------------------------------------
    for(auto i = 0; i < cpuIds.size(); i++){
        // Find the appropriate thread
        int cpuid = cpuIds[i];
        
        int sHist = this->testWkload_glb_worker_thrds[cpuid].stats_.perf_stats.size();
        for(auto j = 0; j < sHist; j++){
            input_qstamp << cpuid << " ";
            PerfCounter hist = this->testWkload_glb_worker_thrds[cpuid].stats_.perf_stats[j];
            input_qstamp << hist.rscan_query.left_ << " "
            << hist.rscan_query.right_ << " "
            << hist.rscan_query.bottom_ << " "
            << hist.rscan_query.top_ << " ";
            for (auto k = 0; k < PERF_EVENT_CNT; k++){
                input_qstamp << hist.raw_counter_values[k] << " ";
            }
            input_qstamp << hist.result;
            input_qstamp << endl;
        }
        if (i == cpuIds.size()-1)
            cout << "==============================================================" << endl;
    }
}

}  //namespace tp
} // namespace erebus
