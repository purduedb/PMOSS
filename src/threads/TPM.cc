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

void TPManager::initWorkerThreads(){
    // -------------------------------------------------------------------------------------
    // initialize worker_threads
    for (unsigned i = 0; i < CURR_WORKER_THREADS; ++i) {
        glb_worker_thrds[worker_cpuids[i]].th = std::thread([i, this]{
            erebus::utils::PinThisThread(worker_cpuids[i]);
            glb_worker_thrds[worker_cpuids[i]].cpuid=worker_cpuids[i];
            
            PerfEvent e;
            static int cnt = 0;

            while (1) {  
                if(!glb_worker_thrds[worker_cpuids[i]].running) {
                    // glb_worker_thrds[worker_cpuids[i]].th.detach();
                    break;
                }
                
                
                Rectangle rec_pop;
                int size_jobqueue = glb_worker_thrds[worker_cpuids[i]].jobs.size();
                
                if (size_jobqueue != 0){
                    glb_worker_thrds[worker_cpuids[i]].jobs.try_pop(rec_pop);
                    /**
                     * TODO: If we have stats for each query, some of the stats value are missed,
                     * actually a lot of the stats value are missed, hence the issue
                     * Added the if condition
                    */
                    // if (cnt % PERF_STAT_COLLECTION_INTERVAL == 0) {
                    //     e.startCounters();
                    // }
                    e.startCounters();
                    // WHICH INDEX?
                    // -------------------------------------------------------------------------------------
                    #if STORAGE == 0
                        int result = QueryRectangle(this->gm->idx, rec_pop.left_, rec_pop.right_, rec_pop.bottom_, rec_pop.top_);
                    #elif STORAGE == 1
                        erebus::storage::qtree::Rect qBox = erebus::storage::qtree::Rect (
                                                                rec_pop.left_,
                                                                rec_pop.bottom_,
                                                                rec_pop.right_ - rec_pop.left_,
                                                                rec_pop.top_ - rec_pop.bottom_ 
                                                            );
                        int result = this->gm->idx_quadtree->getObjectsInBound(qBox);
                    #endif
                    // -------------------------------------------------------------------------------------
                    
                    /**
                     * TODO: If we have stats for each query, some of the stats value are missed,
                     * actually a lot of the stats value are missed, hence the issue
                     * Added the if condition
                    */
                    
                    // cnt +=1;
                    
                    // if (cnt % PERF_STAT_COLLECTION_INTERVAL == (PERF_STAT_COLLECTION_INTERVAL-1)){
                    e.stopCounters();
                    
                    PerfCounter perf_counter;
                    for(auto j=0; j < e.events.size(); j++){
                        if (isnan(e.events[j].readCounter()))
                            perf_counter.raw_counter_values[j] = 0;
                        else
                            perf_counter.raw_counter_values[j] = e.events[j].readCounter();
                    }
                        
                        
                    perf_counter.normalizationConstant = PERF_STAT_COLLECTION_INTERVAL; 
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
                    // cnt = 0;
                    

                    
                    /**
                     * TODO: You should be updating the outstanding queries 
                    */
                    gm->freqQueryDistCompleted[rec_pop.aGrid] += 1;
                    
                    auto itQExecMice = glb_worker_thrds[worker_cpuids[i]].qExecutedMice.find(rec_pop.aGrid);
                    if(itQExecMice != glb_worker_thrds[worker_cpuids[i]].qExecutedMice.end()) 
                        itQExecMice->second += 1;
                    else 
                        glb_worker_thrds[worker_cpuids[i]].qExecutedMice.insert({rec_pop.aGrid, 1});
            
                    // cout << "Threads= " << worker_cpuids[i] << " Result = " << result << " " << rec_pop.validGridIds[0] << endl;
                    // std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                
            }
            
            // glb_worker_thrds[worker_cpuids[i]].th.detach();
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
                if(!glb_megamind_thrds[megamind_cpuids[i]].running) {
                    // glb_megamind_thrds[megamind_cpuids[i]].th.detach();
                    break;
                }
                
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
            // glb_megamind_thrds[megamind_cpuids[i]].th.detach();
        });
    }
}


void TPManager::initSysSweeperThreads(){
    // -------------------------------------------------------------------------------------
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
            
            int rankA = -1, rankB = -1;
            // -------------------------------------------------------------------------------------
            // Params for UPI links
            // std::vector<CoreCounterState> cstates1, cstates2;
            std::vector<SocketCounterState> sktstate1, sktstate2;
            SystemCounterState sstate1, sstate2;
            // -------------------------------------------------------------------------------------
            
            
            PCM * m = PCM::getInstance();
            
            // const PCM::ErrorCode status1 = m->program();
            // m->checkError(status1);
            // Maybe not the default event, only the qpi events
            // const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();

            PCM::ErrorCode status2 = m->programServerUncoreMemoryMetrics(metrics, rankA, rankB);
            m->checkError(status2);
            
            // PCM::ErrorCode returnResult = m->program();
            // if (returnResult != pcm::PCM::Success){
            //     	std::cerr << "Intel's PCM couldn't start" << std::endl;
            //     	std::cerr << "Error code: " << returnResult << std::endl;
            //     	exit(1);
            // }

            uint32 imc_channels = (pcm::uint32)m->getMCChannelsPerSocket();
            uint32 numSockets = m->getNumSockets();
            

            // -------------------------------------------------------------------------------------
            // Params for DRAM Throughput
            
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
                if(!glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].running) {
                    break;
                }
                
                IntelPCMCounter iPCMCnt;

                readState(BeforeState);
                // m->getAllCounterStates(sstate1, sktstate1, cstates1);
                // m->getUncoreCounterStates(sstate2, sktstate2);
                
                BeforeTime = m->getTickCount();
                

                MySleepMs(delay);
                
                AfterTime = m->getTickCount();
                readState(AfterState);
                mDataCh = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns, metrics,
                        show_channel_output, print_update, SPR_CHA_CXL_Event_Count);

                // m->getAllCounterStates(sstate2, sktstate2, cstates2);
                // m->getUncoreCounterStates(sstate2, sktstate2);
                
                // TODO: Need to process these values
                // for (uint32 skt = 0; skt < m->getNumSockets(); ++skt)
                // {
                //     cout << " SKT   " << setw(2) << i << "     ";
                //     for (uint32 l = 0; l < qpiLinks; ++l)
                //         cout << unit_format(getIncomingQPILinkBytes(i, l, sstate1, sstate2)) << "   ";
                //     if (m->qpiUtilizationMetricsAvailable())
                //     {
                //         cout << "|  ";
                //         for (uint32 l = 0; l < qpiLinks; ++l){
                //             iPCMCnt.UPIUtilize[skt][l]   = int(100. * getIncomingQPILinkUtilization(skt, l, sstate1, sstate2));
                //             cout << setw(3) << std::dec << int(100. * getIncomingQPILinkUtilization(i, l, sstate1, sstate2)) << "%   ";
                //         }
                            
                //     }
                //     cout << "\n";
                // }
                    
                
                // TODO: For now skipping the ranks stuff
                // calculate_bandwidth_rank(m, BeforeState, AfterState, AfterTime - BeforeTime, csv, csvheader, 
                //     no_columns, rankA, rankB);

                // TODO: This should be inserted once you make a cycle of collectiing all the rank values
                
                iPCMCnt.sysParams = mDataCh;
                glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].pcmCounters.push(iPCMCnt);
                 
    
                
                swap(BeforeTime, AfterTime);
                swap(BeforeState, AfterState);
                // std::swap(sstate1, sstate2);
                // std::swap(sktstate1, sktstate2);
                // std::swap(cstates1, cstates2);

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
                if(!glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].running) 
                    break;

                std::this_thread::sleep_for(std::chrono::milliseconds(80000));
                // -------------------------------------------------------------------------------------
                // First push the token to the worker cpus to get the DataView
                PerfCounter perf_counter;
                perf_counter.qType = SYNC_TOKEN;
                for (auto[itr, rangeEnd] = this->gm->NUMAToWorkerCPUs.equal_range(numaID); itr != rangeEnd; ++itr)
                {
                    int wkCPUID = itr->second;
                    // cout << itr->first<< '\t' << itr->second << '\n';
                    glb_worker_thrds[wkCPUID].perf_stats.push(perf_counter);
                }

                // -------------------------------------------------------------------------------------
                // Push the token to the system_sweeper cpu to get the System View (MemChannel)
                if (i == 0){
                    IntelPCMCounter iPCMCnt;
                    iPCMCnt.qType = SYNC_TOKEN;
                    glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.push(iPCMCnt);
                }
                
                // -------------------------------------------------------------------------------------
                // Take a snapshot of the DataView from the  threads

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
                            ddSnap.rawCntSamples[pc.gIdx] += PERF_STAT_COLLECTION_INTERVAL; 
                            __m512d rawQCounter[nQCounterCline];
                            __m512d nIns= _mm512_set1_pd (pc.raw_counter_values[1]);
                            for (auto vCline = 0; vCline < nQCounterCline; vCline++){
                                rawQCounter[vCline] = _mm512_load_pd (pc.raw_counter_values + vCline * 8);
                                rawQCounter[vCline] = _mm512_div_pd (rawQCounter[vCline], nIns);
                                rawQCounter[vCline] = _mm512_mul_pd (rawQCounter[vCline], _mm512_set1_pd (1000));
                                if (vCline == 0){
                                    rawQCounter[vCline] = _mm512_mask_blend_pd(0b00000010, rawQCounter[vCline], _mm512_load_pd (pc.raw_counter_values + vCline * 8));
                                }
                                ddSnap.rawQCounter[pc.gIdx][vCline]  = _mm512_add_pd (ddSnap.rawQCounter[pc.gIdx][vCline], rawQCounter[vCline]);
                            }      
                            
   
                        }
                        else
                            break;
                    }
                    
                }
                glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].dataDistReel.push_back(ddSnap);
                
                // -------------------------------------------------------------------------------------
                // Take a snapshot of the QueryExecuted from the worker threads
                struct QueryExecSnap qExecSnap;
                
                for (auto[itr, rangeEnd] = this->gm->NUMAToWorkerCPUs.equal_range(numaID); itr != rangeEnd; ++itr)
                {
                    int wkCPUID = itr->second;   
                    for (auto itQExec : glb_worker_thrds[wkCPUID].qExecutedMice){
                        qExecSnap.qExecutedMice[itQExec.first] += itQExec.second;   
                    }
                }
                glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].queryExecReel.push_back(qExecSnap);

                // -------------------------------------------------------------------------------------
                // -------------------------------------------------------------------------------------
                
                // Calculate the Average
                /**
                 * TODO: This does not tell you the full story: there can be a huge outlier
                 * which can mess up the whole average
                 * Hence the stat function to use here is just absurd I think since
                 * we are doing the dirty work with the data anyway:
                 * Need std_deviation, or just a probabilistic view
                 * */ 
                
                
                // -------------------------------------------------------------------------------------
                // Take a snapshot of the QueryFreq and QueryFreqView and Correlation Matrix from the router threads
                // For now let's assume we have only one router threads/NUMA node
                for (auto[itr, rangeEnd] = this->gm->NUMAToRoutingCPUs.equal_range(numaID); itr != rangeEnd; ++itr)
                {
                    struct QueryViewSnap qViewSnap;
                    int rtCPUID = itr->second;
                    // memcpy(glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].corrQueryReel, glb_router_thrds[rtCPUID].qCorrMatrix, sizeof(glb_router_thrds[rtCPUID].qCorrMatrix));
                    memcpy(qViewSnap.corrQueryReel, glb_router_thrds[rtCPUID].qCorrMatrix, sizeof(glb_router_thrds[rtCPUID].qCorrMatrix));
                    memset(glb_router_thrds[rtCPUID].qCorrMatrix, 0, sizeof(glb_router_thrds[rtCPUID].qCorrMatrix));
                    glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].queryViewReel.push_back(qViewSnap);
                }
                
                // -------------------------------------------------------------------------------------
                // Take a snapshot of the System View (Memory Channel View)
                if (i == 0){
                    bool token_found = false;                    
                    memdata_t DRAMResUsageSnap;
                    while(!token_found){
                        size_t size_stats = glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.unsafe_size();
                        IntelPCMCounter iPCMCnt;
                        if (size_stats != 0){
                            glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.try_pop(iPCMCnt);
                            if (iPCMCnt.qType == SYNC_TOKEN){
                                break;
                            }
                    
                            // Use SIMD to compute the Memory Channel View
                            DRAMResUsageSnap = iPCMCnt.sysParams;
                        }
                        else
                            break;
                    }
                    glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].DRAMResUsageReel.push_back(DRAMResUsageSnap);
                }
                


                
                
            }
            // glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].th.detach();
        });
    }
}

void TPManager::dumpNCoreSweeperThreads(){
    cout << "==========================DUMPING Core Sweeper Threads=======================" << endl;
    for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) {
        #if MACHINE == 0
        #if STORAGE == 0
            string dirName = "/homes/xxxxxxx/works/erebus/kb/" + std::to_string(key);
            mkdir(dirName.c_str(), 0777);
            cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
        #elif STORAGE == 1
            string dirName = "/homes/xxxxxxx/works/erebus/kb_quad/" + std::to_string(key);
            mkdir(dirName.c_str(), 0777);
            cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
        #endif
        #elif MACHINE == 1
        #if STORAGE == 0
            string dirName = "/home/xxxxxxx/works/erebus/home/kb/" + std::to_string(key);
            mkdir(dirName.c_str(), 0777);
            cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
        #elif STORAGE == 1
            string dirName = "/home/xxxxxxx/works/erebus/kb_quad/" + std::to_string(key);
            mkdir(dirName.c_str(), 0777);
            cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
        #endif
        #elif MACHINE == 2
        #if STORAGE == 0
            string dirName = "/home/xxxxxxx/works/erebus/kb_1S/" + std::to_string(key);
            mkdir(dirName.c_str(), 0777);
            cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
        #elif STORAGE == 1
            string dirName = "/home/xxxxxxx/works/erebus/kb_quad_1S/" + std::to_string(key);
            mkdir(dirName.c_str(), 0777);
            cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
        #endif
        #elif MACHINE == 3
        #if STORAGE == 0
            string dirName = "/homes/xxxxxxx/works/erebus/kb_4S/" + std::to_string(key);
            mkdir(dirName.c_str(), 0777);
            cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
        #elif STORAGE == 1
            string dirName = "/homes/xxxxxxx/works/erebus/kb_quad/" + std::to_string(key);
            mkdir(dirName.c_str(), 0777);
            cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
        #endif
        #endif
        
        // -------------------------------------------------------------------------------------
        ofstream memChannelView(dirName + "/mem-channel_view.txt", std::ifstream::app);
        for(size_t i = 0; i < glb_ncore_sweeper_thrds[key].DRAMResUsageReel.size(); i++){
            int tReel = i;
            memChannelView << CONFIG << " ";
            memChannelView << tReel << " ";
            /**
             * TODO: Have a global config header file that saves the value of 
             * global hw params
             * 6 definitely needs to be replaced with such param
             * It should not be numa_num_configured nodes
            */
            // Dump Read Socket Channel
            for (auto sc = 0; sc < 4; sc++){
                for(auto ch = 0; ch < 6; ch++){
                    memChannelView <<  glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].iMC_Rd_socket_chan[sc][ch] << " ";
                }
            }
            // Dump Write Socket Channel
            for (auto sc = 0; sc < 4; sc++){
                for(auto ch = 0; ch < 6; ch++){
                    memChannelView << glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].iMC_Wr_socket_chan[sc][ch] << " ";
                }
            }
            memChannelView << endl;
        }
        // -------------------------------------------------------------------------------------
        ofstream dataView(dirName + "/data_view.txt", std::ifstream::app);
        const int nQCounterCline = PERF_EVENT_CNT/8 + 1;
        // const int scalarDumpSize = MAX_GRID_CELL * nQCounterCline + MAX_GRID_CELL;
        const int scalarDumpSize = MAX_GRID_CELL * nQCounterCline * 8;
        
        alignas(64) double dataViewScalarDump[scalarDumpSize] = {};
        
        for(size_t i = 0; i < glb_ncore_sweeper_thrds[key].dataDistReel.size(); i++){
            int tReel = i;
            DataDistSnap dd = glb_ncore_sweeper_thrds[key].dataDistReel[i];

            dataView << CONFIG << " ";
            dataView << tReel << " ";

            // Load the SIMD values in a memory address
            for (auto g = 0; g < MAX_GRID_CELL; g++){
                for (auto cLine = 0; cLine < nQCounterCline; cLine++){
                    _mm512_store_pd(dataViewScalarDump + (g*nQCounterCline*8)+(cLine*8), dd.rawQCounter[g][cLine]);
                }     
            }
            
            //Dump the perf counters
            for (auto aSize = 0; aSize < scalarDumpSize; aSize++){
                dataView << dataViewScalarDump[aSize] << " ";
            }

            //Dump the sample counts 
            for (auto aSize = 0; aSize < MAX_GRID_CELL; aSize++){
                dataView << dd.rawCntSamples[aSize] << " ";
            }
            
            dataView << endl;
        }

        // -------------------------------------------------------------------------------------
        ofstream queryView(dirName + "/query_view.txt", std::ifstream::app);
        for(size_t i = 0; i < glb_ncore_sweeper_thrds[key].queryViewReel.size(); i++){
            int tReel = i;
            queryView << CONFIG << " ";
            queryView << tReel << " ";
            for(auto aSize1 = 0; aSize1 < MAX_GRID_CELL; aSize1++){
                for(auto aSize2 = 0; aSize2 < MAX_GRID_CELL; aSize2++){
                    queryView << glb_ncore_sweeper_thrds[key].queryViewReel[i].corrQueryReel[aSize1][aSize2] << " ";
                }
            }
            queryView << endl;
        }

        // -------------------------------------------------------------------------------------
        ofstream queryExecView(dirName + "/query-exec_view.txt", std::ifstream::app);
        for(size_t i = 0; i < glb_ncore_sweeper_thrds[key].queryExecReel.size(); i++){
            int tReel = i;
            queryExecView << CONFIG << " ";
            queryExecView << tReel << " ";
            for(auto aSize1 = 0; aSize1 < MAX_GRID_CELL; aSize1++){
                queryExecView << glb_ncore_sweeper_thrds[key].queryExecReel[i].qExecutedMice[aSize1] << " ";
                
            }
            queryExecView << endl;
        }

        // -------------------------------------------------------------------------------------

        cout << "==========================Finished dumping NCore Sweeper Thread =====> " << key <<  endl;
        cout << "-------------------------------------------------------------------------------------"  << endl;
    }    
    cout << "==================================================================" << endl;

}

void TPManager::dumpGridHWCounters(int tID){
    bool isFeaturized[MAX_GRID_CELL] = {0};
    
    for(auto gIdx = 0; gIdx < MAX_GRID_CELL; gIdx++){
        // if (isFeaturized[gIdx] == false) {
            
        //     continue;
        // }
        // -------------------------------------------------------------------------------------
        ofstream input_qstamp("/homes/xxxxxxx/works/erebus/src/qstamp_features/g" + std::to_string(gIdx) + ".txt", std::ifstream::out);
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
        ofstream input_qstamp("/homes/xxxxxxx/works/erebus/src/qstamp_features_th/t" + std::to_string(key) + ".csv", std::ifstream::app);
        
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
        // glb_worker_thrds[key].th.detach();
    }
}

void TPManager::terminateNCoreSweeperThreads(){
    for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) {
        glb_ncore_sweeper_thrds[key].running = false;
        // glb_ncore_sweeper_thrds[key].th.detach();
    }
}


void TPManager::terminateRouterThreads(){
    for (const auto & [ key, value ] : glb_router_thrds) {
        glb_router_thrds[key].running = false;
        // glb_router_thrds[key].th.detach();
    }
}


void TPManager::terminateMegaMindThreads(){
    for (const auto & [ key, value ] : glb_megamind_thrds) {
        glb_megamind_thrds[key].running = false;
        // glb_megamind_thrds[key].th.detach();
    }
}

void TPManager::terminateSysSweeperThreads(){
    for (const auto & [ key, value ] : glb_sys_sweeper_thrds) {
        glb_sys_sweeper_thrds[key].running = false;
        // glb_sys_sweeper_thrds[key].th.detach();
    }
}

void TPManager::detachAllThreads(){
    for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) 
        glb_ncore_sweeper_thrds[key].th.detach();
    

    for (const auto & [ key, value ] : glb_router_thrds)  
        glb_router_thrds[key].th.detach();
    
    for (const auto & [ key, value ] : glb_worker_thrds) 
        glb_worker_thrds[key].th.detach();

    for (const auto & [ key, value ] : glb_megamind_thrds) 
        glb_megamind_thrds[key].th.detach();
    
    for (const auto & [ key, value ] : glb_sys_sweeper_thrds) 
        glb_sys_sweeper_thrds[key].th.detach();
    
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
    ofstream input_qstamp("/homes/xxxxxxx/works/erebus/src/interference/new/mice24_log.txt", std::ifstream::app);
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

TPManager::~TPManager(){
    for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) {
        try
        {
            glb_ncore_sweeper_thrds[key].th.join();
        }
        catch(const std::system_error& e)
        {
            std::cout << "Caught system_error with code "
                    "[" << e.code() << "] meaning "
                    "[" << e.what() << "]\n";
        }
        
    }

    for (const auto & [ key, value ] : glb_router_thrds)  {
        try
        {
            glb_router_thrds[key].th.join();
        }
        catch(const std::system_error& e)
        {
            std::cout << "Caught system_error with code "
                    "[" << e.code() << "] meaning "
                    "[" << e.what() << "]\n";
        }
    }
        
    
    for (const auto & [ key, value ] : glb_worker_thrds) {
        try
        {
            glb_worker_thrds[key].th.join();
        }
        catch(const std::system_error& e)
        {
            std::cout << "Caught system_error with code "
                    "[" << e.code() << "] meaning "
                    "[" << e.what() << "]\n";
        }
    }
        

    for (const auto & [ key, value ] : glb_megamind_thrds) {
        try
        {
            glb_megamind_thrds[key].th.join();
        }
        catch(const std::system_error& e)
        {
            std::cout << "Caught system_error with code "
                    "[" << e.code() << "] meaning "
                    "[" << e.what() << "]\n";
        }
    }
        
    
    for (const auto & [ key, value ] : glb_sys_sweeper_thrds) {
        try
        {
            glb_sys_sweeper_thrds[key].th.join();
        }
        catch(const std::system_error& e)
        {
            std::cout << "Caught system_error with code "
                    "[" << e.code() << "] meaning "
                    "[" << e.what() << "]\n";
        }
    }
}

void TPManager::initRouterThreads(){
    // -------------------------------------------------------------------------------------
    // initialize router threads
    for (unsigned i = 0; i < CURR_ROUTER_THREADS; ++i) {
        glb_router_thrds[router_cpuids[i]].th = std::thread([i, this] {
            erebus::utils::PinThisThread(router_cpuids[i]);
            glb_router_thrds[router_cpuids[i]].cpuid=router_cpuids[i];
            
            // -------------------------------------------------------------------------------------
            // Set the seed and random engine
            std::random_device rd;      // Seed the engine with a random value from the hardware
            std::mt19937 genTem(rd());
            std::mt19937 gen(rd());     // Mersenne Twister 19937 generator
            // std::mt19937 gen(12345); // To make the query workload more deterministic
            
            // -------------------------------------------------------------------------------------
            // Dataset Boundary
            double min_x, max_x, min_y, max_y;
            double max_length, max_width;
            #if DATASET == 0
                // ------------------------US-NORTHEAST----------------------------------------------------
                min_x = -83.478714;
                max_x = -65.87531;
                min_y = 38.78981;
                max_y = 47.491634;
                max_length = 6;  // Previously: 6
                max_width = 6;
            #elif DATASET == 1 
                // ------------------------GEOLITE----------------------------------------------------
                min_x = -179.9695933;
                max_x = 179.9969416;
                min_y = 1.044024;
                max_y = 64.751993; // 400.166666666667;
                max_length = 30;  // Previously: 6
                max_width = 30;
            # elif DATASET == 2
                // ------------------------BMOD02----------------------------------------------------
                min_x = 1308;
                max_x = 12785;
                min_y = 1308;
                max_y = 12785; 
                max_length = 3000;  // Previously: 6
                max_width = 3000;
            #else
                // ------------------------US-NORTHEAST----------------------------------------------------
                min_x = -83.478714;
                max_x = -65.87531;
                min_y = 38.78981;
                max_y = 47.491634;
                max_length = 6;  // Previously: 6
                max_width = 6;
            # endif

    # if WKLOAD == 0
            std::uniform_real_distribution<> dlx(min_x, max_x);
            std::uniform_real_distribution<> dly(min_y, max_y);
            
            // 0 means it will be a point query, however the point may not exist in the dataset
            std::uniform_real_distribution<> dLength(1, max_length);
            std::uniform_real_distribution<> dWidth(1, max_width);
    // Normal workload, where the peak is at the average
    # elif WKLOAD == 1
            // -------------------------------------------------------------------------------------
            # if DATASET == 0
                // -------------------------------US-NORTHEAST---------------------------------------
                double avg_x = (max_x + min_x) / 2;
                double avg_y = (max_y + min_y) / 2;

                double dev_x = (max_x - min_x) / 6;
                double dev_y = (max_y - min_y) / 6;

                // double dev_x = 1.76 / 2;
                // double dev_y = 0.87 / 2;
            # elif DATASET == 1
                // -------------------------------GEOLITE---------------------------------------
                double avg_x = 130;
                double avg_y = 30;
                
                double dev_x = 7;
                double dev_y = 7;
                // -------------------------------------------------------------------------------------
            #endif

            std::normal_distribution<double> dlx(avg_x, dev_x);
            std::normal_distribution<double> dly(avg_y, dev_y);

            std::uniform_real_distribution<> dLength(1, max_length);
            std::uniform_real_distribution<> dWidth(1, max_width);
                
    // Multi-modal with 4 peaks
    # elif WKLOAD == 2
            /**
             * https://stackoverflow.com/questions/37320025/mixture-of-gaussian-distribution-in-c
            */

            using normal_dist   = std::normal_distribution<>;
            using discrete_dist = std::discrete_distribution<std::size_t>;

            # if DATASET == 0
                // -------------------------------US-NORTHEAST---------------------------------------
                // For now change it to sth interesting: random
                double avg_x1 = (-79.95 -78.19) / 2;
                double avg_y1 = (45.75 + 46.62)/ 2;
                double dev_x1 = 3;
                double dev_y1 = 3;

                double avg_x2 = (-79.95 - 76.44) / 2;
                double avg_y2 = (41.00 + 43.14)/ 2;
                double dev_x2 = 3;
                double dev_y2 = 3;

                double avg_x3 = (-72.92 -71.15) / 2;
                double avg_y3 = (43.14 + 44.84)/ 2;
                double dev_x3 = 3;
                double dev_y3 = 3;

                double avg_x4 = -69.35;
                double avg_y4 = (39.65 + 40.53)/ 2;
                double dev_x4 = 3;
                double dev_y4 = 3;
            
            # elif DATASET == 1
                // -------------------------------GEOLITE---------------------------------------
                double avg_x1 = 120;
                double avg_y1 = 20;
                double dev_x1 = 3;
                double dev_y1 = 3;

                double avg_x2 = 140;
                double avg_y2 = 20;
                double dev_x2 = 3;
                double dev_y2 = 3;

                double avg_x3 = 140;
                double avg_y3 = 60;
                double dev_x3 = 3;
                double dev_y3 = 3;

                double avg_x4 = -160;
                double avg_y4 = 20;
                double dev_x4 = 3;
                double dev_y4 = 3;
            # else 
                double dummy_x;
            # endif 

            auto GX = std::array<normal_dist, 4>{
                normal_dist{avg_x1, dev_x1}, // mean, stddev of G[0]
                normal_dist{avg_x2, dev_x2}, // mean, stddev of G[1]
                normal_dist{avg_x3, dev_x3},  // mean, stddev of G[2]
                normal_dist{avg_x4, dev_x4}  // mean, stddev of G[3]
            };
            auto GY = std::array<normal_dist, 4>{
                normal_dist{avg_y1, dev_y1}, // mean, stddev of G[0]
                normal_dist{avg_y2, dev_y2}, // mean, stddev of G[1]
                normal_dist{avg_y3, dev_y3},  // mean, stddev of G[2]
                normal_dist{avg_y4, dev_y4}  // mean, stddev of G[3]
            };

            auto w = discrete_dist{
                0.25, // weight of G[0]
                0.25, // weight of G[1]
                0.25,  // weight of G[2]
                0.25  // weight of G[3]
            };

            // For now change it to sth interesting: random
            std::uniform_real_distribution<> dLength(1, max_length);
            std::uniform_real_distribution<> dWidth(1, max_width);
            
    // Multi-modal with 3 peaks
    # elif WKLOAD == 3
            /**
             * https://stackoverflow.com/questions/37320025/mixture-of-gaussian-distribution-in-c
            */

            using normal_dist   = std::normal_distribution<>;
            using discrete_dist = std::discrete_distribution<std::size_t>;
            # if DATASET == 0
                // -------------------------------US-NORTHEAST---------------------------------------
                double avg_x1 = (-75.65 -69.79) / 2;
                double avg_y1 = (41.69 + 38.79)/ 2;
                double dev_x1 = 2;
                double dev_y1 = 2;

                double avg_x2 = (-71.74 - 65.68) / 2;
                double avg_y2 = (45.56 + 47.49)/ 2;
                double dev_x2 = 2;
                double dev_y2 = 2;

                double avg_x3 = (-83.478714 -65.87531) / 2;
                double avg_y3 = (38.78981 + 47.491634) / 2;
                double dev_x3 = 2;
                double dev_y3 = 2;
            # elif DATASET == 1
                // -------------------------------GEOLITE---------------------------------------
                // double avg_x1 = 120;
                // double avg_y1 = 20;
                // double dev_x1 = 3;
                // double dev_y1 = 3;

                // double avg_x3 = 140;
                // double avg_y3 = 60;
                // double dev_x3 = 3;
                // double dev_y3 = 3;

                // double avg_x2 = -160;
                // double avg_y2 = 20;
                // double dev_x2 = 3;
                // double dev_y2 = 3;
            # endif

            auto GX = std::array<normal_dist, 3>{
                normal_dist{avg_x1, dev_x1}, // mean, stddev of G[0]
                normal_dist{avg_x2, dev_x2}, // mean, stddev of G[1]
                normal_dist{avg_x3, dev_x3} // mean, stddev of G[2]
            };
            auto GY = std::array<normal_dist, 3>{
                normal_dist{avg_y1, dev_y1}, // mean, stddev of G[0]
                normal_dist{avg_y2, dev_y2}, // mean, stddev of G[1]
                normal_dist{avg_y3, dev_y3} // mean, stddev of G[1]
            };

            auto w = discrete_dist{
                0.33, // weight of G[0]
                0.33, // weight of G[1]
                0.34  // weight of G[2]
            };

            std::uniform_real_distribution<> dLength(1, max_length);
            std::uniform_real_distribution<> dWidth(1, max_width);
    // Lookup
    # elif WKLOAD == 4
            int max_objects = this->gm->idx->objects_.size();
            std::uniform_int_distribution<> dob(0, max_objects-1);
    // Zipfian
    #elif WKLOAD == 5
            std::default_random_engine generator;
            erebus::utils::zipfian_int_distribution<int> distX(min_x, max_x, 0.4);
            erebus::utils::zipfian_int_distribution<int> distY(min_y, max_y, 0.4);
            max_length = 6;
            max_width = 6;
            std::uniform_real_distribution<> dLength(1, max_length);
            std::uniform_real_distribution<> dWidth(1, max_width);
    #elif WKLOAD == 6
            using normal_dist   = std::normal_distribution<>;
            using discrete_dist = std::discrete_distribution<std::size_t>;
            const int nHotSpots =3;
            # if DATASET == 0
                // -------------------------------US-NORTHEAST---------------------------------------
                std::vector <std::tuple<double, double>> nPoints = {{-79.9580332, 41.4003572}, {-74.677012, 41.4003572}, {-71.1563312, 42.2705396}};
                std::tuple<double, double> stdDevs = {0.880170200000002, 0.4350911999999987};
            #endif 
            std::array<normal_dist, nHotSpots> GX;
            std::array<normal_dist, nHotSpots> GY;
            for (int spIdx = 0; spIdx < nHotSpots; spIdx++){
                GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs)};
                GY[spIdx] = normal_dist{get<1>(nPoints[spIdx]), get<1>(stdDevs)};
            }
            auto w = discrete_dist{0.25, 0.25, 0.25, 0.25};
            std::uniform_real_distribution<> dlx(min_x, max_x);
            std::uniform_real_distribution<> dly(min_y, max_y);
            
            std::uniform_real_distribution<> dLength(1, 3);
            std::uniform_real_distribution<> dWidth(1, 3);
    #elif WKLOAD == 7
            using normal_dist   = std::normal_distribution<>;
            using discrete_dist = std::discrete_distribution<std::size_t>;
            const int nHotSpots = 5;
            # if DATASET == 0
                // -------------------------------US-NORTHEAST---------------------------------------
                std::vector <std::tuple<double, double>> nPoints = {
                    {-79.9580332, 41.4003572}, {-74.677012, 41.4003572}, {-71.1563312, 42.2705396},
                    {-72.9166716, 44.0109044}, {-69.3959908, 45.7512692}
                    };
                std::tuple<double, double> stdDevs = {0.880170200000002, 0.4350911999999987};
            #endif 
            std::array<normal_dist, nHotSpots> GX;
            std::array<normal_dist, nHotSpots> GY;
            for (int spIdx = 0; spIdx < nHotSpots; spIdx++){
                GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs)};
                GY[spIdx] = normal_dist{get<1>(nPoints[spIdx]), get<1>(stdDevs)};
            }
            auto w = discrete_dist{0.15, 0.15, 0.15, 0.15, 0.15, 0.25};
            std::uniform_real_distribution<> dlx(min_x, max_x);
            std::uniform_real_distribution<> dly(min_y, max_y);
            
            std::uniform_real_distribution<> dLength(1, 3);
            std::uniform_real_distribution<> dWidth(1, 3);        
    #elif WKLOAD == 8
            using normal_dist   = std::normal_distribution<>;
            using discrete_dist = std::discrete_distribution<std::size_t>;
            const int nHotSpots = 7;
            # if DATASET == 0
                // -------------------------------US-NORTHEAST---------------------------------------
                std::vector <std::tuple<double, double>> nPoints = {
                    {-79.9580332, 41.4003572}, {-74.677012, 41.4003572}, {-71.1563312, 42.2705396},
                    {-72.9166716, 44.0109044}, {-69.3959908, 45.7512692},
                    {-78.1976928, 43.140722}, {-76.4373524, 40.5301748}
                    };
                std::tuple<double, double> stdDevs = {0.880170200000002, 0.4350911999999987};
            #endif 
            std::array<normal_dist, nHotSpots> GX;
            std::array<normal_dist, nHotSpots> GY;
            for (int spIdx = 0; spIdx < nHotSpots; spIdx++){
                GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs)};
                GY[spIdx] = normal_dist{get<1>(nPoints[spIdx]), get<1>(stdDevs)};
            }
            auto w = discrete_dist{0.13, 0.13, 0.13, 0.13, 0.13, 0.13, 0.13, 0.09};
            std::uniform_real_distribution<> dlx(min_x, max_x);
            std::uniform_real_distribution<> dly(min_y, max_y);
            
            std::uniform_real_distribution<> dLength(1, 3);
            std::uniform_real_distribution<> dWidth(1, 3);      
    #elif WKLOAD == 9
            using normal_dist   = std::normal_distribution<>;
            using discrete_dist = std::discrete_distribution<std::size_t>;
            auto w = discrete_dist{0.25, 0.75};

            std::vector <std::tuple<double, double>> nPoints = {
                    {-71.9796328, 26.5272116}, {36.0103276, 39.2688054}
                    };
             std::vector <std::tuple<double, double>> stdDevs = {
                    {11.0, 1.3}, {11.0, 4.0}
                    };
            
            std::array<normal_dist, 2> GX;
            std::array<normal_dist, 2> GY;
            for (int spIdx = 0; spIdx < 2; spIdx++){
                GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
                GY[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
            }
    #elif WKLOAD == 20
            using normal_dist   = std::normal_distribution<>;
            using discrete_dist = std::discrete_distribution<std::size_t>;
            auto w = discrete_dist{0.50, 0.50};

            std::vector <std::tuple<double, double>> nPoints = {
                    {-71.9796328, 26.5272116}, {36.0103276, 39.2688054}
                    };
             std::vector <std::tuple<double, double>> stdDevs = {
                    {11.0, 1.3}, {11.0, 4.0}
                    };
            
            std::array<normal_dist, 2> GX;
            std::array<normal_dist, 2> GY;
            for (int spIdx = 0; spIdx < 2; spIdx++){
                GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
                GY[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
            }

    #elif WKLOAD == 21
            using normal_dist   = std::normal_distribution<>;
            using discrete_dist = std::discrete_distribution<std::size_t>;
            auto w = discrete_dist{0.75, 0.25};

            std::vector <std::tuple<double, double>> nPoints = {
                    {-71.9796328, 26.5272116}, {36.0103276, 39.2688054}
                    };
             std::vector <std::tuple<double, double>> stdDevs = {
                    {11.0, 1.3}, {11.0, 4.0}
                    };
            
            std::array<normal_dist, 2> GX;
            std::array<normal_dist, 2> GY;
            for (int spIdx = 0; spIdx < 2; spIdx++){
                GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
                GY[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
            }

    // Log Normal 
    # else
            # if DATASET == 0
                // -------------------------------US-NORTHEAST---------------------------------------
                double pseudo_min_x = 1;
                double pseudo_max_x = 1 + (max_x - min_x);
                double avg_x = (log(pseudo_max_x) + log(pseudo_min_x)) / 2;
                double avg_y = (log(max_y) + log(min_y)) / 2;

                double dev_x = (log(pseudo_max_x) - log(pseudo_min_x)) / 6;
                double dev_y = (log(max_y) - log(min_y)) / 6;

            # elif DATASET == 1
                // -------------------------------GEOLITE---------------------------------------
                // double avg_x = 130;
                // double avg_y = 30;
                
                // double dev_x = 10;
                // double dev_y = 10;
            # endif

            std::lognormal_distribution<double> dlx(avg_x, dev_x);
            std::lognormal_distribution<double> dly(avg_y, dev_y);

            std::uniform_real_distribution<> dLength(1, max_length);
            std::uniform_real_distribution<> dWidth(1, max_width);
    # endif
    // -------------------------------------------------------------------------------------
            while (1) {
                if(!glb_router_thrds[router_cpuids[i]].running) {
                    // glb_router_thrds[router_cpuids[i]].th.detach();
                    break;
                }
                
    // Uniform workload
    #if WKLOAD == 0
                double lx = dlx(gen);
                double ly = dly(gen);

                double length = dLength(gen);
                double width = dWidth(gen);
                
                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);
    
    // Normal workload, where the peak is at the average
    #elif WKLOAD == 1
                double lx = dlx(gen);
                double ly = dly(gen);

                double length = dLength(gen);
                double width = dWidth(gen);
                
                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);

    // Multi-modal with 4 peaks
    #elif WKLOAD == 2
                auto indexX = w(genTem);
                double lx = GX[indexX](genTem);
                
                auto indexY = w(genTem);
                double ly = GY[indexY](genTem);
                
                double length = dLength(gen);
                double width = dWidth(gen);

                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);
    
    // Multi-modal with 3 peaks
    #elif WKLOAD == 3
                auto indexX = w(genTem);
                double lx = GX[indexX](genTem);
                
                auto indexY = w(genTem);
                double ly = GY[indexY](genTem);
                
                double length = dLength(gen);
                double width = dWidth(gen);
                
                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);

    #elif WKLOAD == 4
                int idx_to_search = dob(gen);
                double lx = this->gm->idx->objects_[idx_to_search]->left_;
                double ly = this->gm->idx->objects_[idx_to_search]->bottom_;
                
                double hx = lx;
                double hy = ly;
                
                Rectangle query(lx, hx, ly, hy);
    #elif WKLOAD == 5
                
                double lx = distX(generator);
                double ly = distY(generator);
                
                double length = dLength(gen);
                double width = dWidth(gen);
                
                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);
    #elif WKLOAD == 6
                auto index = w(genTem); // which hotspot to choose?
                double lx, ly;
                if (index == nHotSpots) {
                    lx = dlx(gen);
                    ly = dly(gen);
                }
                else{
                    lx = GX[index](genTem) + 0.880170200000002;
                    ly = GY[index](genTem) - 0.4350911999999987;
                }
                
                double length = dLength(gen);
                double width = dWidth(gen);
                
                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);
    #elif WKLOAD == 7
                auto index = w(genTem); // which hotspot to choose?
                double lx, ly;
                if (index == nHotSpots) {
                    lx = dlx(gen);
                    ly = dly(gen);
                }
                else{
                    lx = GX[index](genTem) + 0.880170200000002;
                    ly = GY[index](genTem) - 0.4350911999999987;
                }
                
                double length = dLength(gen);
                double width = dWidth(gen);
                
                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);
    #elif WKLOAD == 8
                auto index = w(genTem); // which hotspot to choose?
                double lx, ly;
                if (index == nHotSpots) {
                    lx = dlx(gen);
                    ly = dly(gen);
                }
                else{
                    lx = GX[index](genTem) + 0.880170200000002;
                    ly = GY[index](genTem) - 0.4350911999999987;
                }
                
                double length = dLength(gen);
                double width = dWidth(gen);
                
                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);
    #elif WKLOAD == 9
                auto index = w(genTem); // which hotspot to choose?
                double lx, ly, hx, hy;
                if (index == 0) { // this is a point search
                    lx = GX[index](genTem);
                    ly = GY[index](genTem);
                    hx = lx + 0;
                    hy = ly + 0;
                }
                else{
                    lx = GX[index](genTem);
                    ly = GY[index](genTem);

                    hx = lx + 4;
                    hy = ly + 2;
                }
                Rectangle query(lx, hx, ly, hy);

    #elif WKLOAD == 20
                auto index = w(genTem); // which hotspot to choose?
                double lx, ly, hx, hy;
                if (index == 0) { // this is a point search
                    lx = GX[index](genTem);
                    ly = GY[index](genTem);
                    hx = lx + 0;
                    hy = ly + 0;
                }
                else{
                    lx = GX[index](genTem);
                    ly = GY[index](genTem);

                    hx = lx + 4;
                    hy = ly + 2;
                }
                Rectangle query(lx, hx, ly, hy);
    #elif WKLOAD == 21
                auto index = w(genTem); // which hotspot to choose?
                double lx, ly, hx, hy;
                if (index == 0) { // this is a point search
                    lx = GX[index](genTem);
                    ly = GY[index](genTem);
                    hx = lx + 0;
                    hy = ly + 0;
                }
                else{
                    lx = GX[index](genTem);
                    ly = GY[index](genTem);

                    hx = lx + 4;
                    hy = ly + 2;
                }
                Rectangle query(lx, hx, ly, hy);
    // Log Normal 
    #else
                double lx = dlx(gen);
                double ly = dly(gen);
                while(lx > pseudo_max_x || lx < pseudo_min_x)
                    lx = dlx(gen);
                while(ly > max_y || ly < min_y)
                    ly = dly(gen);
                
                lx = lx - (1 - min_x);

                double length = dLength(gen);
                double width = dWidth(gen);
                
                double hx = lx + length;
                double hy = ly + width;

                Rectangle query(lx, hx, ly, hy);
    #endif

                // -------------------------------------------------------------------------------------
                // Check which grid the query belongs to 
                std::vector<int> valid_gcells;
    #if LINUX == 3
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
    #else
                for (auto gc = 0; gc < gm->nGridCells; gc++){
                    valid_gcells.push_back(gc); 
                    query.validGridIds.push_back(gc); // May not be necessary
                }
                    
    #endif 
                
                // -------------------------------------------------------------------------------------
                // Check the sanity of the query
                if (valid_gcells.size() == 0) continue;
                // -------------------------------------------------------------------------------------
                // TODO: Update the Query Correlation Matrix
                for(size_t qc1 = 0; qc1 < valid_gcells.size()-1; qc1++){
                    int pCell = valid_gcells[qc1];
                    for(size_t qc2 = qc1; qc2 < valid_gcells.size(); qc2++){
                        int cCell = valid_gcells[qc2];
                        glb_router_thrds[router_cpuids[i]].qCorrMatrix[pCell][cCell] ++;
                        glb_router_thrds[router_cpuids[i]].qCorrMatrix[cCell][pCell] ++;
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

}  //namespace tp
} // namespace erebus
