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

void TPManager::init_worker_threads(){
  for (unsigned i = 0; i < CURR_WORKER_THREADS; ++i) {
    glb_worker_thrds[worker_cpuids[i]].th = std::thread([i, this]{
      erebus::utils::PinThisThread(worker_cpuids[i]);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      glb_worker_thrds[worker_cpuids[i]].cpuid=worker_cpuids[i];
          
      PerfEvent e;
      int cnt = 0;
      
      // ofstream outfile;
      // outfile.open ("/homes/yrayhan/works/erebus/src/a_test/" + std::to_string(worker_cpuids[i]) + "example.txt");
      while (1) {  
        if(!glb_worker_thrds[worker_cpuids[i]].running) {
            break;
        }
                    
        int result = 0;        
        Rectangle rec_pop;
        int size_jobqueue = glb_worker_thrds[worker_cpuids[i]].jobs.size();
        
        // ycsb workload: holds lookup result
        std::vector<uint64_t> v; 
        v.reserve(10);
                    
        if (size_jobqueue != 0){
          glb_worker_thrds[worker_cpuids[i]].jobs.try_pop(rec_pop);
          if (cnt == 0) {
            e.startCounters();
          }
          // e.startCounters();
                    
          // -------------------------------------------------------------------------------------
          #if STORAGE == 0
            result = QueryRectangle(this->gm->idx, rec_pop.left_, rec_pop.right_, rec_pop.bottom_, rec_pop.top_);
          #elif STORAGE == 1
            erebus::storage::qtree::Rect qBox = erebus::storage::qtree::Rect ( rec_pop.left_,
              rec_pop.bottom_,
              rec_pop.right_ - rec_pop.left_,
              rec_pop.top_ - rec_pop.bottom_ 
            );
            result = this->gm->idx_quadtree->getObjectsInBound(qBox);
          #elif STORAGE == 2  
            if(rec_pop.op == ycsbc::Operation::INSERT){
              result = this->gm->idx_btree->insert(static_cast<uint64_t>(rec_pop.left_), static_cast<uint64_t>(rec_pop.bottom_));
            }
            else if(rec_pop.op == ycsbc::Operation::SCAN){
              result = this->gm->idx_btree->scan(static_cast<uint64_t>(rec_pop.left_), static_cast<int>(rec_pop.right_));
            }
            else if(rec_pop.op == ycsbc::Operation::READ){
              v.clear();
              result = this->gm->idx_btree->find(static_cast<uint64_t>(rec_pop.left_), &v);
            }
            else{
              cout << "ycsb operation does not match" << endl;
            }
            // outfile << static_cast<uint64_t>(rec_pop.left_) << ' ' << static_cast<uint64_t>(rec_pop.right_) << ' '
            //   << static_cast<uint64_t>(rec_pop.bottom_) << endl;
          #endif
          
          cnt +=1;
          if (cnt == PERF_STAT_COLLECTION_INTERVAL){
            e.stopCounters();
            cnt=0;
            PerfCounter perf_counter;
            for(auto j=0; j < e.events.size(); j++){
              if (isnan(e.events[j].readCounter()) || isinf(e.events[j].readCounter())) perf_counter.raw_counter_values[j] = 0;
              else perf_counter.raw_counter_values[j] = e.events[j].readCounter();
            }
                              
            perf_counter.normalizationConstant = PERF_STAT_COLLECTION_INTERVAL; 
            perf_counter.rscan_query = rec_pop;
            perf_counter.result = result;
            perf_counter.gIdx = rec_pop.aGrid;
                              
            glb_worker_thrds[worker_cpuids[i]].perf_stats.push(perf_counter);

          }
          
                    
          // PerfCounter perf_counter;
          // for(auto j=0; j < e.events.size(); j++){
          //   if (isnan(e.events[j].readCounter()) || isinf(e.events[j].readCounter())) perf_counter.raw_counter_values[j] = 0;
          //   else perf_counter.raw_counter_values[j] = e.events[j].readCounter();
          // }
                            
          // perf_counter.normalizationConstant = PERF_STAT_COLLECTION_INTERVAL; 
          // perf_counter.rscan_query = rec_pop;
          // perf_counter.result = result;
          // perf_counter.gIdx = rec_pop.aGrid;
                            
          // glb_worker_thrds[worker_cpuids[i]].perf_stats.push(perf_counter);
          
                          
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

void TPManager::init_megamind_threads(){
  // -------------------------------------------------------------------------------------
  for (unsigned i = 0; i < CURR_MEGAMIND_THREADS; ++i) {
    glb_megamind_thrds[megamind_cpuids[i]].th = std::thread([i, this] {
      erebus::utils::PinThisThread(megamind_cpuids[i]);
      glb_megamind_thrds[megamind_cpuids[i]].cpuid=megamind_cpuids[i];
      int numaID = numa_node_of_cpu(megamind_cpuids[i]);
      while (1) {
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

// void TPManager::init_syssweeper_threads(){
//   // -------------------------------------------------------------------------------------
//   for (unsigned i = 0; i < CURR_SYS_SWEEPER_THREADS; ++i) {
//     glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].th = std::thread([i, this] {
//       erebus::utils::PinThisThread(sys_sweeper_cpuids[i]);
//       glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].cpuid=sys_sweeper_cpuids[i];
            
//         // -------------------------------------------------------------------------------------
//         // Params for DRAM Throughput
//         double delay = 30000;
//         bool csv = false, csvheader = false, show_channel_output = true, print_update = false;
//         uint32 no_columns = DEFAULT_DISPLAY_COLUMNS; // Default number of columns is 2
        
//         ServerUncoreMemoryMetrics metrics = PartialWrites;
//         int rankA = -1, rankB = -1;
//         // -------------------------------------------------------------------------------------
//         // Params for UPI links
//         std::vector<CoreCounterState> cstates1, cstates2;
//         std::vector<SocketCounterState> sktstate1, sktstate2;
//         SystemCounterState sstate1, sstate2;
//         // -------------------------------------------------------------------------------------
            
            
//         PCM * m = PCM::getInstance();
//         PCM::ErrorCode status2 = m->programServerUncoreMemoryMetrics(metrics, rankA, rankB);
//         m->checkError(status2);
        
        
//         const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();
//         uint32 imc_channels = (pcm::uint32)m->getMCChannelsPerSocket();
//         uint32 numSockets = m->getNumSockets();

//         m->getUncoreCounterStates(sstate1, sktstate1);
//         // m->getAllCounterStates(sstate1, sktstate1, cstates1);
        
//         // -------------------------------------------------------------------------------------
//         // Params for DRAM Throughput
            
//         uint64 SPR_CHA_CXL_Event_Count = 0;
//         rankA = 0;
//         rankB = 1;
//         std::vector<ServerUncoreCounterState> BeforeState(m->getNumSockets());
//         std::vector<ServerUncoreCounterState> AfterState(m->getNumSockets());
//         // -------------------------------------------------------------------------------------

//         memdata_t mDataCh;
//         uint64 BeforeTime = 0, AfterTime = 0;            
//         while (1) {
//           if(!glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].running) {
//               break;
//           }
            
//           IntelPCMCounter iPCMCnt;
//           readState(BeforeState);            
//           BeforeTime = m->getTickCount();
//           MySleepMs(delay);
//           AfterTime = m->getTickCount();
//           readState(AfterState);
//           m->getUncoreCounterStates(sstate2, sktstate2);          
//           // m->getAllCounterStates(sstate1, sktstate1, cstates1);

//           mDataCh = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns, metrics,
//             show_channel_output, print_update, SPR_CHA_CXL_Event_Count);

          
//           if (m->getNumSockets() > 1 && m->incomingQPITrafficMetricsAvailable()){
//             for (uint32 skt = 0; skt < m->getNumSockets(); ++skt){
//               for (uint32 l = 0; l < qpiLinks; ++l){
//                 iPCMCnt.upi_incoming[skt][l] = getIncomingQPILinkBytes(skt, l, sstate1, sstate2);
//               }
//               // TODO: the getQPILinkSpeed returns 0, hence all the methods that use this function return bad result.
//             }
//             iPCMCnt.upi_system[0] = getAllIncomingQPILinkBytes(sstate1, sstate2);
//             iPCMCnt.upi_system[1] = getQPItoMCTrafficRatio(sstate1, sstate2);
//           } 
              
//           if (m->getNumSockets() > 1 && (m->outgoingQPITrafficMetricsAvailable())){ // QPI info only for multi socket systems
//             for (uint32 skt = 0; skt < m->getNumSockets(); ++skt){
//                 for (uint32 l = 0; l < qpiLinks; ++l){
//                   iPCMCnt.upi_outgoing[skt][l] = getMyOutgoingQPILinkBytes(skt, l, sstate1, sstate2);
//                 }
//             }
//             iPCMCnt.upi_system[2] = getAllOutgoingQPILinkBytes(sstate1, sstate2);
//           }
          
//           // TODO: For now skipping the ranks stuff
//           // calculate_bandwidth_rank(m, BeforeState, AfterState, AfterTime - BeforeTime, csv, csvheader, 
//           //     no_columns, rankA, rankB);
            
//           iPCMCnt.sysParams = mDataCh;
//           glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].pcmCounters.push(iPCMCnt);
              

            
//           swap(BeforeTime, AfterTime);
//           swap(BeforeState, AfterState);
//           std::swap(sstate1, sstate2);
//           std::swap(sktstate1, sktstate2);
        
//           if(rankA == 6) rankA = 0;
//           else rankA += 2;
          
//           if(rankB == 7) rankB = 1;
//           else rankB += 2;      

//         }
//         });
//   }
// }


void TPManager::init_ncoresweeper_threads(){
  for (unsigned i = 0; i < CURR_NCORE_SWEEPER_THREADS; ++i) {
    glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].th = std::thread([i, this] {
      erebus::utils::PinThisThread(ncore_sweeper_cpuids[i]);
      glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].cpuid=ncore_sweeper_cpuids[i];
      int numaID = numa_node_of_cpu(ncore_sweeper_cpuids[i]);
      while (1) 
      {
        if(!glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].running) 
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(40000));  // 80000
        
        // First, push the token to the worker cpus to get the DataView
        PerfCounter perf_counter;
        perf_counter.qType = SYNC_TOKEN;
        for (auto[itr, rangeEnd] = this->gm->NUMAToWorkerCPUs.equal_range(numaID); itr != rangeEnd; ++itr)
        {
          int wkCPUID = itr->second;
          // cout << itr->first<< '\t' << itr->second << '\n';
          glb_worker_thrds[wkCPUID].perf_stats.push(perf_counter);
        }
        
        // Then, push the token to the system_sweeper cpu to get the System View (MemChannel)
        // if (i == 0){
        //   IntelPCMCounter iPCMCnt;
        //   iPCMCnt.qType = SYNC_TOKEN;
        //   glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.push(iPCMCnt);
        // }
        
        // Take a snapshot of the DataView from the  threads
        const int nQCounterCline = PERF_EVENT_CNT/8 + PERF_EVENT_CNT%8;
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
                
                
                ddSnap.rawCntSamples[pc.gIdx] += PERF_STAT_COLLECTION_INTERVAL; 
                for(auto ex = 0; ex < PERF_EVENT_CNT; ex++){
                  ddSnap.rawQCounter[pc.gIdx][ex] += (pc.raw_counter_values[ex] / pc.raw_counter_values[1])*1000;
                }
                ddSnap.rawQCounter[pc.gIdx][1] = pc.raw_counter_values[1];

                // Use SIMD to compute the DataView
                // __m512d rawQCounter[nQCounterCline];
                // __m512d nIns= _mm512_set1_pd (pc.raw_counter_values[1]);
                // for (auto vCline = 0; vCline < nQCounterCline; vCline++){
                //   rawQCounter[vCline] = _mm512_load_pd (pc.raw_counter_values + vCline * 8);
                //   rawQCounter[vCline] = _mm512_div_pd (rawQCounter[vCline], nIns);
                //   rawQCounter[vCline] = _mm512_mul_pd (rawQCounter[vCline], _mm512_set1_pd (1000));
                //   if (vCline == 0){
                //     rawQCounter[vCline] = _mm512_mask_blend_pd(0b00000010, rawQCounter[vCline], _mm512_load_pd (pc.raw_counter_values + vCline * 8));
                //   }
                //   ddSnap.rawQCounter[pc.gIdx][vCline]  = _mm512_add_pd (ddSnap.rawQCounter[pc.gIdx][vCline], rawQCounter[vCline]);
                // }      
                

            }
            else break;
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
            // new addition to only count last round
            // glb_worker_thrds[wkCPUID].qExecutedMice.clear();
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
        // if (i == 0){
        //     bool token_found = false;                    
        //     // memdata_t DRAMResUsageSnap;
        //     IntelPCMCounter DRAMResUsageSnap;
        //     while(!token_found){
        //         size_t size_stats = glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.unsafe_size();
        //         IntelPCMCounter iPCMCnt;
        //         if (size_stats != 0){
        //             glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.try_pop(iPCMCnt);
        //             if (iPCMCnt.qType == SYNC_TOKEN){
        //                 break;
        //             }
            
        //             // Use SIMD to compute the Memory Channel View
        //             // DRAMResUsageSnap = iPCMCnt.sysParams;
        //             DRAMResUsageSnap = iPCMCnt;
        //         }
        //         else
        //             break;
        //     }
        //     glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].DRAMResUsageReel.push_back(DRAMResUsageSnap);
        // }
        
      }
      // glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].th.detach();
    });
  }
}

void TPManager::dump_ncoresweeper_threads(){
  cout << "==========================DUMPING Core Sweeper Threads=======================" << endl;
  for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) {

  string dirName = std::string(PROJECT_SOURCE_DIR);
  #if STORAGE == 0
      dirName += "/kb/" + std::to_string(key);
  #elif STORAGE == 1
      dirName += "/kb_quad/" + std::to_string(key);
  #elif STORAGE == 2
      dirName += "/kb_b/" + std::to_string(key);
  #endif
  
  mkdir(dirName.c_str(), 0777);
  cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
        // -------------------------------------------------------------------------------------
    ofstream memChannelView(dirName + "/mem-channel_view.txt", std::ifstream::app);
    // for(size_t i = 0; i < glb_ncore_sweeper_thrds[key].DRAMResUsageReel.size(); i++){
    //     int tReel = i;
    //     memChannelView << this->gm->config << " ";
    //     memChannelView << tReel << " ";
    //     memChannelView << this->gm->wkload << " ";
    //     memChannelView << this->gm->iam << " ";
    //     /**
    //      * TODO: Have a global config header file that saves the value of 
    //      * global hw params
    //      * 6 definitely needs to be replaced with such param
    //      * It should not be numa_num_configured nodes
    //     */
    //     // Dump Read Socket Channel
    //     for (auto sc = 0; sc < 4; sc++){
    //         for(auto ch = 0; ch < 6; ch++){
    //             memChannelView <<  glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].sysParams.iMC_Rd_socket_chan[sc][ch] << " ";
    //         }
    //     }
    //     // Dump Write Socket Channel
    //     for (auto sc = 0; sc < 4; sc++){
    //         for(auto ch = 0; ch < 6; ch++){
    //             memChannelView << glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].sysParams.iMC_Wr_socket_chan[sc][ch] << " ";
    //         }
    //     }
    //     // // Dump Write Socket Channel
    //     for (auto sc = 0; sc < 4; sc++){
    //         for(auto ul = 0; ul < 3; ul++){
    //             memChannelView << glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].upi_incoming[sc][ul] << " ";
    //         }
    //     }
    //     // Dump Write Socket Channel
    //     for (auto sc = 0; sc < 4; sc++){
    //         for(auto ul = 0; ul < 3; ul++){
    //             memChannelView << glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].upi_outgoing[sc][ul] << " ";
    //         }
    //     }
    //     memChannelView << endl;
    // }
    // -------------------------------------------------------------------------------------
    ofstream dataView(dirName + "/data_view.txt", std::ifstream::app);
    const int nQCounterCline = PERF_EVENT_CNT/8 + PERF_EVENT_CNT%8;
    // const int scalarDumpSize = MAX_GRID_CELL * nQCounterCline * 8;
    const int scalarDumpSize = MAX_GRID_CELL * PERF_EVENT_CNT;
    
    alignas(64) double dataViewScalarDump[scalarDumpSize] = {};
        
    for(size_t i = 0; i < glb_ncore_sweeper_thrds[key].dataDistReel.size(); i++){
        int tReel = i;
        DataDistSnap dd = glb_ncore_sweeper_thrds[key].dataDistReel[i];

        dataView << this->gm->config  << " ";
        dataView << tReel << " ";
        dataView << this->gm->wkload << " ";
        dataView << this->gm->iam << " ";
        
        for (auto g = 0; g < MAX_GRID_CELL; g++){
          memcpy(dataViewScalarDump+g*PERF_EVENT_CNT, dd.rawQCounter[g], sizeof(dd.rawQCounter[g]));
        }

        // Load the SIMD values in a memory address
        // for (auto g = 0; g < MAX_GRID_CELL; g++){
        //     for (auto cLine = 0; cLine < nQCounterCline; cLine++){
        //         _mm512_store_pd(dataViewScalarDump + (g*nQCounterCline*8)+(cLine*8), dd.rawQCounter[g][cLine]);
        //     }     
        // }
        
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
        queryView << this->gm->config  << " ";
        queryView << tReel << " ";
        queryView << this->gm->wkload << " ";
        queryView << this->gm->iam << " ";
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
        queryExecView << this->gm->config  << " ";
        queryExecView << tReel << " ";
        queryExecView << this->gm->wkload  << " ";
        queryExecView << this->gm->iam  << " ";
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

void TPManager::terminate_worker_threads(){
  for (const auto & [ key, value ] : glb_worker_thrds) {
    glb_worker_thrds[key].running = false;
    // glb_worker_thrds[key].th.detach();
  }
}

void TPManager::terminate_ncoresweeper_threads(){
  for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) {
    glb_ncore_sweeper_thrds[key].running = false;
    // glb_ncore_sweeper_thrds[key].th.detach();
  }
}


void TPManager::terminate_router_threads(){
  for (const auto & [ key, value ] : glb_router_thrds) {
    glb_router_thrds[key].running = false;
    // glb_router_thrds[key].th.detach();
  }
}


void TPManager::terminate_megamind_threads(){
  for (const auto & [ key, value ] : glb_megamind_thrds) {
    glb_megamind_thrds[key].running = false;
    // glb_megamind_thrds[key].th.detach();
  }
}

void TPManager::terminate_syssweeper_threads(){
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


void TPManager::terminateTestWorkerThreads(){
    for (const auto & [ key, value ] : testWkload_glb_worker_thrds) {
        testWkload_glb_worker_thrds[key].running = false;
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

void TPManager::init_router_threads(int ds, int wl, double min_x, double max_x, double min_y, double max_y,
    std::vector<keytype> &init_keys, std::vector<uint64_t> &values){
  
  for (unsigned i = 0; i < CURR_ROUTER_THREADS; ++i) {
    glb_router_thrds[router_cpuids[i]].th = std::thread([i, this, ds, wl, min_x, max_x, min_y, max_y, &init_keys, &values] {
    erebus::utils::PinThisThread(router_cpuids[i]);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    glb_router_thrds[router_cpuids[i]].cpuid=router_cpuids[i];
    
    
    double pseudo_min_x = 1;
    double pseudo_max_x = 1 + (max_x - min_x);
    // -------------------------------------------------------------------------------------
    std::random_device rd;      
    std::mt19937 genTem(rd());
    std::mt19937 gen(rd());     
    std::uniform_real_distribution<> dlx_ureal;
    std::uniform_real_distribution<> dly_ureal;
    std::uniform_real_distribution<> dLength_ureal;
    std::uniform_real_distribution<> dWidth_ureal;
    std::normal_distribution<double> dlx_norm;
    std::normal_distribution<double> dly_norm;
    std::uniform_int_distribution<> dob_uint;
    std::default_random_engine generator;
    erebus::utils::zipfian_int_distribution<int> dlx_zipint;
    erebus::utils::zipfian_int_distribution<int> dly_zipint;
    using normal_dist   = std::normal_distribution<>;
    using discrete_dist = std::discrete_distribution<std::size_t>;
    std::array<normal_dist, 10> GX;
    std::array<normal_dist, 10> GY;
    discrete_dist w;
    std::lognormal_distribution<double> dlx_lnorm;
    std::lognormal_distribution<double> dly_lnorm;
    // b tree experiments
    std::uniform_int_distribution<uint64_t> dx_uint64;  
    std::uniform_int_distribution<uint64_t> dLength_uint64;  


    ycsbc::utils::Properties props;
    ycsbc::CoreWorkload ycsb_wl;
    
    // -------------------------------------------------------------------------------------
    double max_length, max_width;

    if (ds == OSM_USNE){
      max_length = 6; max_width = 6;
    }
    else if (ds == GEOLITE){
      max_length = 30; max_width = 30;
    }
    else if(ds == BERLINMOD02){
      max_length = 3000; max_width = 3000;
    } 
    
    if (wl == MD_RS_UNIFORM){
      dlx_ureal = std::uniform_real_distribution<>(min_x, max_x);
      dly_ureal = std::uniform_real_distribution<>(min_y, max_y);
      dLength_ureal = std::uniform_real_distribution<> (1, max_length);
      dWidth_ureal = std::uniform_real_distribution<> (1, max_width);
    }
    else if (wl == MD_RS_NORMAL){
      double avg_x, avg_y, dev_x, dev_y;
      if (ds == OSM_USNE){
        avg_x = (max_x + min_x) / 2;
        avg_y = (max_y + min_y) / 2;
        dev_x = (max_x - min_x) / 6;
        dev_y = (max_y - min_y) / 6;
      }
      else if (ds == GEOLITE){
        avg_x = 130;
        avg_y = 30;
        dev_x = 7;
        dev_y = 7;
      }
      dlx_norm = std::normal_distribution<double> (avg_x, dev_x);
      dly_norm = std::normal_distribution<double> (avg_y, dev_y);
      dLength_ureal = std::uniform_real_distribution<> (1, max_length);
      dWidth_ureal = std::uniform_real_distribution<> (1, max_width);	
    }
    else if (wl == MD_LK_UNIFORM){
      int max_objects = this->gm->idx->objects_.size();
      dob_uint = std::uniform_int_distribution<>(0, max_objects-1);
    }  
    else if (wl == MD_RS_ZIPF){
      dlx_zipint = erebus::utils::zipfian_int_distribution<int>(min_x, max_x, 0.4);
      dly_zipint = erebus::utils::zipfian_int_distribution<int>(min_y, max_y, 0.4);
      max_length = 6;
      max_width = 6;
      dLength_ureal = std::uniform_real_distribution<> (1, max_length);
      dWidth_ureal = std::uniform_real_distribution<> (1, max_width);	
    }
    else if (wl == MD_RS_HOT3){
      const int nHotSpots =3;
      std::vector <std::tuple<double, double>> nPoints;
      std::tuple<double, double> stdDevs;
      if (ds == OSM_USNE){
          // -------------------------------US-NORTHEAST---------------------------------------
          nPoints = {{-79.9580332, 41.4003572}, {-74.677012, 41.4003572}, {-71.1563312, 42.2705396}};
          stdDevs = {0.880170200000002, 0.4350911999999987};
      }
      for (int spIdx = 0; spIdx < nHotSpots; spIdx++){
          GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs)};
          GY[spIdx] = normal_dist{get<1>(nPoints[spIdx]), get<1>(stdDevs)};
      }
      w = discrete_dist{0.25, 0.25, 0.25, 0.25};
      dlx_ureal = std::uniform_real_distribution<> (min_x, max_x);
      dly_ureal = std::uniform_real_distribution<> (min_y, max_y);
      dLength_ureal = std::uniform_real_distribution<> (1, 3);
      dWidth_ureal = std::uniform_real_distribution<> (1, 3);	

    }
    else if (wl == MD_RS_HOT5){
      std::vector <std::tuple<double, double>> nPoints;
      std::tuple<double, double> stdDevs;

      const int nHotSpots = 5;
      if (ds == OSM_USNE){
          // -------------------------------US-NORTHEAST---------------------------------------
          nPoints = {
              {-79.9580332, 41.4003572}, {-74.677012, 41.4003572}, {-71.1563312, 42.2705396},
              {-72.9166716, 44.0109044}, {-69.3959908, 45.7512692}
              };
          stdDevs = {0.880170200000002, 0.4350911999999987};
      }
      for (int spIdx = 0; spIdx < nHotSpots; spIdx++){
          GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs)};
          GY[spIdx] = normal_dist{get<1>(nPoints[spIdx]), get<1>(stdDevs)};
      }
      w = discrete_dist{0.15, 0.15, 0.15, 0.15, 0.15, 0.25};
      dlx_ureal = std::uniform_real_distribution<> (min_x, max_x);
      dly_ureal = std::uniform_real_distribution<> (min_y, max_y);
      dLength_ureal = std::uniform_real_distribution<> (1, 3);
      dWidth_ureal = std::uniform_real_distribution<> (1, 3);	
    }        
    else if (wl == MD_RS_HOT7){
      std::vector <std::tuple<double, double>> nPoints;
      std::tuple<double, double> stdDevs;
      const int nHotSpots = 7;
      if (ds == OSM_USNE){
          // -------------------------------US-NORTHEAST---------------------------------------
          nPoints = {
              {-79.9580332, 41.4003572}, {-74.677012, 41.4003572}, {-71.1563312, 42.2705396},
              {-72.9166716, 44.0109044}, {-69.3959908, 45.7512692},
              {-78.1976928, 43.140722}, {-76.4373524, 40.5301748}
              };
          stdDevs = {0.880170200000002, 0.4350911999999987};
      }
      
      for (int spIdx = 0; spIdx < nHotSpots; spIdx++){
          GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs)};
          GY[spIdx] = normal_dist{get<1>(nPoints[spIdx]), get<1>(stdDevs)};
      }
      w = discrete_dist{0.13, 0.13, 0.13, 0.13, 0.13, 0.13, 0.13, 0.09};
      dlx_ureal = std::uniform_real_distribution<> (min_x, max_x);
      dly_ureal = std::uniform_real_distribution<> (min_y, max_y);
      dLength_ureal = std::uniform_real_distribution<> (1, 3);
      dWidth_ureal = std::uniform_real_distribution<> (1, 3);	
    }    
    else if (wl == MD_LK_RS_25_75){
      w = discrete_dist{0.25, 0.75};

      std::vector <std::tuple<double, double>> nPoints = {
              {-71.9796328, 26.5272116}, {36.0103276, 39.2688054}
              };
        std::vector <std::tuple<double, double>> stdDevs = {
              {11.0, 1.3}, {11.0, 4.0}
              };
      
      for (int spIdx = 0; spIdx < 2; spIdx++){
          GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
          GY[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
      }
    }
    else if(wl == MD_LK_RS_50_50){
      w = discrete_dist{0.50, 0.50};

      std::vector <std::tuple<double, double>> nPoints = {
              {-71.9796328, 26.5272116}, {36.0103276, 39.2688054}
              };
        std::vector <std::tuple<double, double>> stdDevs = {
              {11.0, 1.3}, {11.0, 4.0}
              };
      
      for (int spIdx = 0; spIdx < 2; spIdx++){
          GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
          GY[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
      }
    }
    else if (wl == MD_LK_RS_75_25){
      w = discrete_dist{0.75, 0.25};

      std::vector <std::tuple<double, double>> nPoints = {
              {-71.9796328, 26.5272116}, {36.0103276, 39.2688054}
              };
      std::vector <std::tuple<double, double>> stdDevs = {
            {11.0, 1.3}, {11.0, 4.0}
            };
      
      for (int spIdx = 0; spIdx < 2; spIdx++){
          GX[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
          GY[spIdx] = normal_dist{get<0>(nPoints[spIdx]), get<0>(stdDevs[spIdx])};
      }
    }
    else if (wl == MD_RS_LOGNORMAL){
      double avg_x, avg_y, dev_x, dev_y;
      if (ds == OSM_USNE){
        
        avg_x = (log(pseudo_max_x) + log(pseudo_min_x)) / 2;
        avg_y = (log(max_y) + log(min_y)) / 2;

        dev_x = (log(pseudo_max_x) - log(pseudo_min_x)) / 6;
        dev_y = (log(max_y) - log(min_y)) / 6;
      }
      else if (ds == GEOLITE){
        // double avg_x = 130;
        // double avg_y = 30;
        
        // double dev_x = 10;
        // double dev_y = 10;
      }
      dlx_lnorm = std::lognormal_distribution<double> (avg_x, dev_x);
      dly_lnorm = std::lognormal_distribution<double> (avg_y, dev_y);
      dLength_ureal  = std::uniform_real_distribution<> (1, max_length);
      dWidth_ureal = std::uniform_real_distribution<> (1, max_width);
    }
    else if (
      wl == SD_YCSB_WKLOADA || wl == SD_YCSB_WKLOADC || wl == SD_YCSB_WKLOADE ||
      wl == SD_YCSB_WKLOADF || wl == SD_YCSB_WKLOADE1 || wl == SD_YCSB_WKLOADH || 
      wl == SD_YCSB_WKLOADI ||
      wl == WIKI_WKLOADA || wl == WIKI_WKLOADC || wl == WIKI_WKLOADE || wl == WIKI_WKLOADI || 
      wl == WIKI_WKLOADH || wl == WIKI_WKLOADA1 || wl == WIKI_WKLOADA2 || wl == WIKI_WKLOADA3
    ){
      // for inserts open different keyrange config for different router
      // or use a single router
      std::ifstream input;
      #if MACHINE==2
      std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/2s_2n/";
      #elif MACHINE==3
      std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/2s_8n/";
      #endif
      
      if (wl == SD_YCSB_WKLOADA){
        wl_config += "ycsb_workloada_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADC){
        wl_config += "ycsb_workloadc";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADE){
        wl_config += "ycsb_workloade_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADF){
        wl_config += "ycsb_workloadf_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADE1){
        wl_config += "ycsb_workloade1_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADH){
        wl_config += "ycsb_workloadh";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADI){
        wl_config += "ycsb_workloadi";
        input.open(wl_config);
      }
      else if(wl == WIKI_WKLOADA){
        wl_config += "wiki_workloada_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if(wl == WIKI_WKLOADA1){
        wl_config += "wiki_workloada1_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if(wl == WIKI_WKLOADA2){
        wl_config += "wiki_workloada2_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if(wl == WIKI_WKLOADA3){
        wl_config += "wiki_workloada3_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if(wl == WIKI_WKLOADC){
        wl_config += "wiki_workloadc";
        input.open(wl_config);
      }
      else if(wl == WIKI_WKLOADE){
        wl_config += "wiki_workloade_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if(wl == WIKI_WKLOADI){
        wl_config += "wiki_workloadi";
        input.open(wl_config);
      }
      else if(wl == WIKI_WKLOADH){
        wl_config += "wiki_workloadh";
        input.open(wl_config);
      }
      else{
        cerr << "ycsb workload not recognized" << endl;
      }
      
      try {
        props.Load(input);
      } catch (const std::string &message) {
        std::cerr << message << std::endl;
      }
      input.close();
      ycsb_wl.Init(props);
    }

    // -------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------
    while (1) {
      if(!glb_router_thrds[router_cpuids[i]].running) {
          break;
      }
                
      // Query parameters 
      double lx, ly, hx, hy, length, width;
      Rectangle query;
      uint64_t tx_keys[3] = {0};

    if(wl == MD_RS_UNIFORM){
      lx = dlx_ureal(gen);
      ly = dly_ureal(gen);
      length = dLength_ureal(gen);
      width = dLength_ureal(gen);
      hx = lx + length;
      hy = ly + width;
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (wl == MD_RS_NORMAL){  
      lx = dlx_norm(gen);
      ly = dly_norm(gen);
      length = dLength_ureal(gen);
      width = dLength_ureal(gen);
      hx = lx + length;
      hy = ly + width;
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (wl == MD_LK_UNIFORM){
      int idx_to_search = dob_uint(gen);
      lx = this->gm->idx->objects_[idx_to_search]->left_;
      ly = this->gm->idx->objects_[idx_to_search]->bottom_;
      hx = lx;
      hy = ly;
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (wl == MD_RS_ZIPF){
      lx = dlx_zipint(generator);
      ly = dly_zipint(generator);
      length = dLength_ureal(gen);
      width = dLength_ureal(gen);
      hx = lx + length;
      hy = ly + width;
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (wl == MD_RS_HOT3){
      const int nHotSpots = 3;
      auto index = w(genTem); // which hotspot to choose?
      if (index == nHotSpots) {
          lx = dlx_ureal(gen);
          ly = dly_ureal(gen);
      }
      else{
          lx = GX[index](genTem) + 0.880170200000002;
          ly = GY[index](genTem) - 0.4350911999999987;
      }  
      length = dLength_ureal(gen);
      width = dLength_ureal(gen);
      hx = lx + length;
      hy = ly + width;
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (wl == MD_RS_HOT5){
      const int nHotSpots = 5;
      auto index = w(genTem); // which hotspot to choose?
      if (index == nHotSpots) {
          lx = dlx_ureal(gen);
          ly = dly_ureal(gen);
      }
      else{
          lx = GX[index](genTem) + 0.880170200000002;
          ly = GY[index](genTem) - 0.4350911999999987;
      }
      length = dLength_ureal(gen);
      width = dLength_ureal(gen);
      hx = lx + length;
      hy = ly + width;
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (wl == MD_RS_HOT7){
      const int nHotSpots = 7;
      auto index = w(genTem); // which hotspot to choose?
      if (index == nHotSpots) {
          lx = dlx_ureal(gen);
          ly = dly_ureal(gen);
      }
      else{
          lx = GX[index](genTem) + 0.880170200000002;
          ly = GY[index](genTem) - 0.4350911999999987;
      }
      length = dLength_ureal(gen);
      width = dLength_ureal(gen);
      hx = lx + length;
      hy = ly + width;
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (wl == MD_LK_RS_25_75){
      auto index = w(genTem); // which hotspot to choose?
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
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (wl == MD_LK_RS_50_50){
      auto index = w(genTem); // which hotspot to choose?
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
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (wl == MD_LK_RS_75_25){
      auto index = w(genTem); // which hotspot to choose?
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
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (wl == MD_RS_LOGNORMAL){
      lx = dlx_lnorm(gen);
      ly = dly_lnorm(gen);
      
      while(lx > pseudo_max_x || lx < pseudo_min_x)
          lx = dlx_lnorm(gen);
      while(ly > max_y || ly < min_y)
          ly = dly_lnorm(gen);  
      lx = lx - (1 - min_x);
      length = dLength_ureal(gen);
      width = dWidth_ureal(gen);
      hx = lx + length;
      hy = ly + width;
      query = Rectangle(lx, hx, ly, hy);
    }
    else if (
      wl == SD_YCSB_WKLOADA || wl == SD_YCSB_WKLOADC || wl == SD_YCSB_WKLOADE ||
      wl == SD_YCSB_WKLOADF || wl == SD_YCSB_WKLOADE1 || wl == SD_YCSB_WKLOADH || wl == SD_YCSB_WKLOADI ||
      wl == WIKI_WKLOADA || wl == WIKI_WKLOADC || wl == WIKI_WKLOADE || wl == WIKI_WKLOADI || 
      wl == WIKI_WKLOADH || wl == WIKI_WKLOADA1 || wl == WIKI_WKLOADA2 || wl == WIKI_WKLOADA3
      ){
      ycsb_wl.DoTransaction(tx_keys);  
      uint64_t value = -1;
      
      lx = init_keys[tx_keys[0]]; // The key to insert/search/update
      length = tx_keys[1];  // in case of range scan
      if (tx_keys[2] == ycsbc::Operation::INSERT) value = values[tx_keys[0]];  // in case of 
      query = Rectangle(lx, length, value, -1);
      query.op = tx_keys[2];
      // cout << tx_keys[0] << ' ' << tx_keys[2] << endl;
    }

      // -------------------------------------------------------------------------------------
      // Check which grid the query belongs to 
      std::vector<int> valid_gcells;
      #if LINUX == 3
        for (auto gc = 0; gc < gm->nGridCells; gc++){  
          double glx = gm->glbGridCell[gc].lx;
          double gly = gm->glbGridCell[gc].ly;
          double ghx = gm->glbGridCell[gc].hx;
          double ghy = gm->glbGridCell[gc].hy;
          #if MULTIDIM == 1
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
          #else
            if (lx <= ghx && lx >= glx){
              valid_gcells.push_back(gc);  
              query.validGridIds.push_back(gc);
            }
            else continue;
          #endif 
              
        }
      #else
        for (auto gc = 0; gc < gm->nGridCells; gc++){
          valid_gcells.push_back(gc); 
          query.validGridIds.push_back(gc); // May not be necessary
        }                
      #endif 
      
      // Check the sanity of the query       
      if (valid_gcells.size() == 0) continue;  
      
      // Update the Query Correlation Matrix
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
      
    // Push the query to the correct worker thread's job queue
    std::mt19937 genInt(rd());
    std::uniform_int_distribution<int> dq(0, valid_gcells.size()-1); 
    int insert_tid = dq(genInt);

    int glbGridCellInsert = valid_gcells[insert_tid];
                
      // Stamping the query with something: You need to do the inverse of log_2
    /*
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
    */
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
        // Use it as a throttling factor
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
            
    });
  }
}

}  //namespace tp
} // namespace erebus
