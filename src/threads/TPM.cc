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

  // for(size_t i=0; i<worker_cpuids.size();i++){
  //     WorkerThread wt;
  //     wt.th=std::thread();
  //     wt.cpuid = worker_cpuids[i];
  //     glb_worker_thrds[worker_cpuids[i]] = wt;
  // }
  
}

void TPManager::init_worker_threads(){

  for (unsigned i = 0; i < CURR_WORKER_THREADS; ++i) {
    // worker_mutex.lock();
    glb_worker_thrds[worker_cpuids[i]].th = std::thread([i, this]{
      erebus::utils::PinThisThread(worker_cpuids[i]);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      glb_worker_thrds[worker_cpuids[i]].cpuid=worker_cpuids[i];
      // worker_mutex.unlock();

      #if PROFILE==1
      PerfEvent e;
      int cnt = 0;  
      #endif

      while (1) {  
        // std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if(!glb_worker_thrds[worker_cpuids[i]].running) {
            break;
        }
        
        Rectangle rec_pop;
        int size_jobqueue = glb_worker_thrds[worker_cpuids[i]].jobs.size();
        
        // ycsb workload: holds lookup result
        std::vector<uint64_t> v; 
        v.reserve(10);
                    
        if (size_jobqueue != 0){
          glb_worker_thrds[worker_cpuids[i]].jobs.try_pop(rec_pop);
          
          #if PROFILE==1
          if (cnt == 0) {
            e.startCounters();
          }
          #endif

          int result=0;
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
          
          #if PROFILE == 1
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
          #endif 

          gm->freqQueryDistCompleted[rec_pop.aGrid] += 1;

          auto itQExecMice = glb_worker_thrds[worker_cpuids[i]].qExecutedMice.find(rec_pop.aGrid);
          if(itQExecMice != glb_worker_thrds[worker_cpuids[i]].qExecutedMice.end()) 
            itQExecMice->second += 1;
          else 
            glb_worker_thrds[worker_cpuids[i]].qExecutedMice.insert({rec_pop.aGrid, 1});

      }
                
    }
                
    });
  }
}


void TPManager::init_ncoresweeper_threads(){
  for (unsigned i = 0; i < CURR_NCORE_SWEEPER_THREADS; ++i) {
    glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].th = std::thread([i, this] {
      erebus::utils::PinThisThread(ncore_sweeper_cpuids[i]);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].cpuid=ncore_sweeper_cpuids[i];
      int numaID = numa_node_of_cpu(ncore_sweeper_cpuids[i]);
      while (1) 
      {
        if(!glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].running) 
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(40000));  // 80000
        
        #if PROFILE == 1
        // First, push the token to the worker cpus to get the DataView
        PerfCounter perf_counter;
        perf_counter.qType = SYNC_TOKEN;
        for (auto[itr, rangeEnd] = this->gm->NUMAToWorkerCPUs.equal_range(numaID); itr != rangeEnd; ++itr)
        {
          int wkCPUID = itr->second;
          glb_worker_thrds[wkCPUID].perf_stats.push(perf_counter);
        }
        
        #if MACHINE==0
        // Then, push the token to the system_sweeper cpu to get the System View (MemChannel)
        if (i == 0){
          IntelPCMCounter iPCMCnt;
          iPCMCnt.qType = SYNC_TOKEN;
          glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.push(iPCMCnt);
        }
        #endif
        
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
        #endif

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

        #if PROFILE == 1
        #if MACHINE == 0
        // -------------------------------------------------------------------------------------
        // Take a snapshot of the System View (Memory Channel View)
        if (i == 0){
            bool token_found = false;                    
            // memdata_t DRAMResUsageSnap;
            IntelPCMCounter DRAMResUsageSnap;
            while(!token_found){
                size_t size_stats = glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.unsafe_size();
                IntelPCMCounter iPCMCnt;
                if (size_stats != 0){
                    glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.try_pop(iPCMCnt);
                    if (iPCMCnt.qType == SYNC_TOKEN){
                        break;
                    }
            
                    // Use SIMD to compute the Memory Channel View
                    // DRAMResUsageSnap = iPCMCnt.sysParams;
                    DRAMResUsageSnap = iPCMCnt;
                }
                else
                    break;
            }
            glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].DRAMResUsageReel.push_back(DRAMResUsageSnap);
        }
        #endif
        #endif 


        
      }
      // glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].th.detach();
    });
  }
}

void TPManager::dump_ncoresweeper_threads(){
  cout << "==========================DUMPING Core Sweeper Threads=======================" << endl;
  for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) {

  string dirName = std::string(PROJECT_SOURCE_DIR);
  #if PROFILE==0
  #if STORAGE == 0
      dirName += "/kb_r_linux/" + std::to_string(key);
  #elif STORAGE == 1
      dirName += "/kb_quad_linux/" + std::to_string(key);
  #elif STORAGE == 2
      dirName += "/kb_b_linux/" + std::to_string(key);
  #endif
  #else
  #if STORAGE == 0
      dirName += "/kb_r_linux_pfl/" + std::to_string(key);
  #elif STORAGE == 1
      dirName += "/kb_quad_linux_pfl/" + std::to_string(key);
  #elif STORAGE == 2
      dirName += "/kb_b_linux_pfl/" + std::to_string(key);
  #endif
  #endif
  
  mkdir(dirName.c_str(), 0777);
  cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
    // -------------------------------------------------------------------------------------
    
    #if PROFILE == 1
    // -------------------------------------------------------------------------------------
    ofstream memChannelView(dirName + "/mem-channel_view.txt", std::ifstream::app);
    #if MACHINE==0
    for(size_t i = 0; i < glb_ncore_sweeper_thrds[key].DRAMResUsageReel.size(); i++){
        int tReel = i;
        memChannelView << this->gm->config << " ";
        memChannelView << tReel << " ";
        memChannelView << this->gm->wkload << " ";
        memChannelView << this->gm->iam << " ";
        /**
         * TODO: Have a global config header file that saves the value of 
         * global hw params
         * 6 definitely needs to be replaced with such param
         * It should not be numa_num_configured nodes
        */
        // Dump Read Socket Channel
        for (auto sc = 0; sc < 4; sc++){
            for(auto ch = 0; ch < 6; ch++){
                memChannelView <<  glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].sysParams.iMC_Rd_socket_chan[sc][ch] << " ";
            }
        }
        // Dump Write Socket Channel
        for (auto sc = 0; sc < 4; sc++){
            for(auto ch = 0; ch < 6; ch++){
                memChannelView << glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].sysParams.iMC_Wr_socket_chan[sc][ch] << " ";
            }
        }
        // // Dump Write Socket Channel
        for (auto sc = 0; sc < 4; sc++){
            for(auto ul = 0; ul < 3; ul++){
                memChannelView << glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].upi_incoming[sc][ul] << " ";
            }
        }
        // Dump Write Socket Channel
        for (auto sc = 0; sc < 4; sc++){
            for(auto ul = 0; ul < 3; ul++){
                memChannelView << glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].upi_outgoing[sc][ul] << " ";
            }
        }
        memChannelView << endl;
    }
    #endif
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
    #endif 

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


void TPManager::init_syssweeper_threads(){
  #if MACHINE==0
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
        std::vector<CoreCounterState> cstates1, cstates2;
        std::vector<SocketCounterState> sktstate1, sktstate2;
        SystemCounterState sstate1, sstate2;
        // -------------------------------------------------------------------------------------
            
            
        PCM * m = PCM::getInstance();
        PCM::ErrorCode status2 = m->programServerUncoreMemoryMetrics(metrics, rankA, rankB);
        m->checkError(status2);
        
        
        const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();
        uint32 imc_channels = (pcm::uint32)m->getMCChannelsPerSocket();
        uint32 numSockets = m->getNumSockets();

        m->getUncoreCounterStates(sstate1, sktstate1);
        // m->getAllCounterStates(sstate1, sktstate1, cstates1);
        
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
        while (1) {
          if(!glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].running) {
              break;
          }
            
          IntelPCMCounter iPCMCnt;
          readState(BeforeState);            
          BeforeTime = m->getTickCount();
          MySleepMs(delay);
          AfterTime = m->getTickCount();
          readState(AfterState);
          m->getUncoreCounterStates(sstate2, sktstate2);          
          // m->getAllCounterStates(sstate1, sktstate1, cstates1);

          mDataCh = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns, metrics,
            show_channel_output, print_update, SPR_CHA_CXL_Event_Count);

          
          if (m->getNumSockets() > 1 && m->incomingQPITrafficMetricsAvailable()){
            for (uint32 skt = 0; skt < m->getNumSockets(); ++skt){
              for (uint32 l = 0; l < qpiLinks; ++l){
                iPCMCnt.upi_incoming[skt][l] = getIncomingQPILinkBytes(skt, l, sstate1, sstate2);
              }
              // TODO: the getQPILinkSpeed returns 0, hence all the methods that use this function return bad result.
            }
            iPCMCnt.upi_system[0] = getAllIncomingQPILinkBytes(sstate1, sstate2);
            iPCMCnt.upi_system[1] = getQPItoMCTrafficRatio(sstate1, sstate2);
          } 
              
          if (m->getNumSockets() > 1 && (m->outgoingQPITrafficMetricsAvailable())){ // QPI info only for multi socket systems
            for (uint32 skt = 0; skt < m->getNumSockets(); ++skt){
                for (uint32 l = 0; l < qpiLinks; ++l){
                  iPCMCnt.upi_outgoing[skt][l] = getMyOutgoingQPILinkBytes(skt, l, sstate1, sstate2);
                }
            }
            iPCMCnt.upi_system[2] = getAllOutgoingQPILinkBytes(sstate1, sstate2);
          }
          
          // TODO: For now skipping the ranks stuff
          // calculate_bandwidth_rank(m, BeforeState, AfterState, AfterTime - BeforeTime, csv, csvheader, 
          //     no_columns, rankA, rankB);
            
          iPCMCnt.sysParams = mDataCh;
          glb_sys_sweeper_thrds[sys_sweeper_cpuids[i]].pcmCounters.push(iPCMCnt);
              

            
          swap(BeforeTime, AfterTime);
          swap(BeforeState, AfterState);
          std::swap(sstate1, sstate2);
          std::swap(sktstate1, sktstate2);
        
          if(rankA == 6) rankA = 0;
          else rankA += 2;
          
          if(rankB == 7) rankB = 1;
          else rankB += 2;      

        }
        });
  }
  #endif
}


void TPManager::init_router_threads(int ds, int wl, double min_x, double max_x, double min_y, double max_y,
    std::vector<keytype> &init_keys, std::vector<uint64_t> &values, std::string machine_name){
  
  std::mutex router_mutex;
  for (unsigned i = 0; i < CURR_ROUTER_THREADS; ++i) {  
    glb_router_thrds[router_cpuids[i]].th = std::thread([i, this, ds, wl, min_x, max_x, min_y, max_y, 
      &init_keys, &values, machine_name, &router_mutex] {

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

    std::mt19937 genInt(rd());
    std::uniform_int_distribution<int> dq(0, CURR_WORKER_THREADS-1);    
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
      wl == SD_YCSB_WKLOADI || wl == SD_YCSB_WKLOADA1 || wl == SD_YCSB_WKLOADH11 || 
      wl == WIKI_WKLOADA || wl == WIKI_WKLOADC || wl == WIKI_WKLOADE || wl == WIKI_WKLOADI || 
      wl == WIKI_WKLOADH || wl == WIKI_WKLOADA1 || wl == WIKI_WKLOADA2 || wl == WIKI_WKLOADA3 ||
      wl == OSM_WKLOADA || wl == OSM_WKLOADC || wl == OSM_WKLOADE || wl == OSM_WKLOADH || wl == OSM_WKLOADA0 ||
      wl == SD_YCSB_WKLOADH1 || wl == SD_YCSB_WKLOADH2 || wl == SD_YCSB_WKLOADH3 || wl == SD_YCSB_WKLOADH4 || wl == SD_YCSB_WKLOADH5 ||
      wl == SD_YCSB_WKLOADA00 || wl == SD_YCSB_WKLOADA01 || wl == SD_YCSB_WKLOADC1 || wl == SD_YCSB_WKLOADK4
    ){
      // for inserts open different keyrange config for different router
      // or use a single router
      std::ifstream input;
      std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/" + machine_name + "/";
      
      if (wl == SD_YCSB_WKLOADA){
        wl_config += "ycsb_workloada_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADA00){
        wl_config += "ycsb_workloada00_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADA01){
        wl_config += "ycsb_workloada01_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADC){
        wl_config += "ycsb_workloadc";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADC1){
        wl_config += "ycsb_workloadc1";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADK4){
        wl_config += "ycsb_workloadk4";
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
      else if (wl == SD_YCSB_WKLOADH1){
        wl_config += "ycsb_workloadh1";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADH11){
        wl_config += "ycsb_workloadh11";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADH2){
        wl_config += "ycsb_workloadh2";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADH3){
        wl_config += "ycsb_workloadh3";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADH4){
        wl_config += "ycsb_workloadh4";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADH5){
        wl_config += "ycsb_workloadh5";
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
      else if(wl == OSM_WKLOADA){
        wl_config += "osm_workloada_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if(wl == OSM_WKLOADA0){
        wl_config += "osm_workloada0_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if(wl == OSM_WKLOADC){
        wl_config += "osm_workloadc";
        input.open(wl_config);
      }
      else if(wl == OSM_WKLOADE){
        wl_config += "osm_workloade_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if(wl == OSM_WKLOADH){
        wl_config += "osm_workloadh";
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
      wl == SD_YCSB_WKLOADF || wl == SD_YCSB_WKLOADE1 || wl == SD_YCSB_WKLOADH || 
      wl == SD_YCSB_WKLOADI || wl == SD_YCSB_WKLOADA1 || wl == SD_YCSB_WKLOADH11 || 
      wl == WIKI_WKLOADA || wl == WIKI_WKLOADC || wl == WIKI_WKLOADE || wl == WIKI_WKLOADI || 
      wl == WIKI_WKLOADH || wl == WIKI_WKLOADA1 || wl == WIKI_WKLOADA2 || wl == WIKI_WKLOADA3 ||
      wl == OSM_WKLOADA || wl == OSM_WKLOADC || wl == OSM_WKLOADE || wl == OSM_WKLOADH || wl == OSM_WKLOADA0 ||
      wl == SD_YCSB_WKLOADH1 || wl == SD_YCSB_WKLOADH2 || wl == SD_YCSB_WKLOADH3 || wl == SD_YCSB_WKLOADH4 || wl == SD_YCSB_WKLOADH5 ||
      wl == SD_YCSB_WKLOADA00 || wl == SD_YCSB_WKLOADA01 || wl == SD_YCSB_WKLOADC1 || wl == SD_YCSB_WKLOADK4 
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

  
      std::vector<int> valid_gcells;
      for (auto gc = 0; gc < gm->nGridCells; gc++){  
        double glx = gm->glbGridCell[gc].lx;
        double gly = gm->glbGridCell[gc].ly;
        double ghx = gm->glbGridCell[gc].hx;
        double ghy = gm->glbGridCell[gc].hy;
        #if MULTIDIM == 1
          if (hx < glx || lx > ghx || hy < gly || ly > ghy)
            continue;
          else {
            valid_gcells.push_back(gc);  
            query.validGridIds.push_back(gc);
          }
        #else
          if (lx <= ghx && lx >= glx){
            valid_gcells.push_back(gc);  
            query.validGridIds.push_back(gc);
          }
          else 
            continue; 
        #endif
      }
                    
      if (valid_gcells.size() == 0) continue;  
      
      
      for(size_t qc1 = 0; qc1 < valid_gcells.size()-1; qc1++){
          int pCell = valid_gcells[qc1];
          for(size_t qc2 = qc1; qc2 < valid_gcells.size(); qc2++){
              int cCell = valid_gcells[qc2];
              glb_router_thrds[router_cpuids[i]].qCorrMatrix[pCell][cCell] ++;
              glb_router_thrds[router_cpuids[i]].qCorrMatrix[cCell][pCell] ++;
          }
      }

      if(this->gm->config >= 500 && this->gm->config <= 505){
        valid_gcells ={
          0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
        };
      }
      else if(this->gm->config == 506){
        std::vector<std::vector<int>> sn_numa;
        #if MACHINE==0
          // sn_numa={{ 0 , 31 }, { 32 , 63 }, { 64 , 95 }, { 96 , 127 }, { 128 , 159 }, { 160 , 191 }, { 192 , 223 }, { 224 , 255 }};
          sn_numa ={
            {0 ,1 ,2 ,3 ,4 ,5 ,6 ,7 ,8 ,9 ,10 ,11 ,12 ,13 ,14 ,15 ,16 ,17 ,18 ,19 ,20 ,21 ,22 ,23 ,24 ,25 ,26 ,27 ,28 ,29 ,30 ,31}, 
            {32 ,33 ,34 ,35 ,36 ,37 ,38 ,39 ,40 ,41 ,42 ,43 ,44 ,45 ,46 ,47 ,48 ,49 ,50 ,51 ,52 ,53 ,54 ,55 ,56 ,57 ,58 ,59 ,60 ,61 ,62 ,63}, 
            {64 ,65 ,66 ,67 ,68 ,69 ,70 ,71 ,72 ,73 ,74 ,75 ,76 ,77 ,78 ,79 ,80 ,81 ,82 ,83 ,84 ,85 ,86 ,87 ,88 ,89 ,90 ,91 ,92 ,93 ,94 ,95}, 
            {96 ,97 ,98 ,99 ,100 ,101 ,102 ,103 ,104 ,105 ,106 ,107 ,108 ,109 ,110 ,111 ,112 ,113 ,114 ,115 ,116 ,117 ,118 ,119 ,120 ,121 ,122 ,123 ,124 ,125 ,126 ,127}, 
            {128 ,129 ,130 ,131 ,132 ,133 ,134 ,135 ,136 ,137 ,138 ,139 ,140 ,141 ,142 ,143 ,144 ,145 ,146 ,147 ,148 ,149 ,150 ,151 ,152 ,153 ,154 ,155 ,156 ,157 ,158 ,159}, 
            {160 ,161 ,162 ,163 ,164 ,165 ,166 ,167 ,168 ,169 ,170 ,171 ,172 ,173 ,174 ,175 ,176 ,177 ,178 ,179 ,180 ,181 ,182 ,183 ,184 ,185 ,186 ,187 ,188 ,189 ,190 ,191}, 
            {192 ,193 ,194 ,195 ,196 ,197 ,198 ,199 ,200 ,201 ,202 ,203 ,204 ,205 ,206 ,207 ,208 ,209 ,210 ,211 ,212 ,213 ,214 ,215 ,216 ,217 ,218 ,219 ,220 ,221 ,222 ,223}, 
            {224 ,225 ,226 ,227 ,228 ,229 ,230 ,231 ,232 ,233 ,234 ,235 ,236 ,237 ,238 ,239 ,240 ,241 ,242 ,243 ,244 ,245 ,246 ,247 ,248 ,249 ,250 ,251 ,252 ,253 ,254 ,255}
          };
        #elif MACHINE==1
          // sn_numa={{ 0 , 127 }, { 128 , 255 }};
          sn_numa = {
            {0 ,1 ,2 ,3 ,4 ,5 ,6 ,7 ,8 ,9 ,10 ,11 ,12 ,13 ,14 ,15 ,16 ,17 ,18 ,19 ,20 ,21 ,22 ,23 ,24 ,25 ,26 ,27 ,28 ,29 ,30 ,31 ,32 ,33 ,34 ,35 ,36 ,37 ,38 ,39 ,40 ,41 ,42 ,43 ,44 ,45 ,46 ,47 ,48 ,49 ,50 ,51 ,52 ,53 ,54 ,55 ,56 ,57 ,58 ,59 ,60 ,61 ,62 ,63 ,64 ,65 ,66 ,67 ,68 ,69 ,70 ,71 ,72 ,73 ,74 ,75 ,76 ,77 ,78 ,79 ,80 ,81 ,82 ,83 ,84 ,85 ,86 ,87 ,88 ,89 ,90 ,91 ,92 ,93 ,94 ,95 ,96 ,97 ,98 ,99 ,100 ,101 ,102 ,103 ,104 ,105 ,106 ,107 ,108 ,109 ,110 ,111 ,112 ,113 ,114 ,115 ,116 ,117 ,118 ,119 ,120 ,121 ,122 ,123 ,124 ,125 ,126 ,127}, 
            {128 ,129 ,130 ,131 ,132 ,133 ,134 ,135 ,136 ,137 ,138 ,139 ,140 ,141 ,142 ,143 ,144 ,145 ,146 ,147 ,148 ,149 ,150 ,151 ,152 ,153 ,154 ,155 ,156 ,157 ,158 ,159 ,160 ,161 ,162 ,163 ,164 ,165 ,166 ,167 ,168 ,169 ,170 ,171 ,172 ,173 ,174 ,175 ,176 ,177 ,178 ,179 ,180 ,181 ,182 ,183 ,184 ,185 ,186 ,187 ,188 ,189 ,190 ,191 ,192 ,193 ,194 ,195 ,196 ,197 ,198 ,199 ,200 ,201 ,202 ,203 ,204 ,205 ,206 ,207 ,208 ,209 ,210 ,211 ,212 ,213 ,214 ,215 ,216 ,217 ,218 ,219 ,220 ,221 ,222 ,223 ,224 ,225 ,226 ,227 ,228 ,229 ,230 ,231 ,232 ,233 ,234 ,235 ,236 ,237 ,238 ,239 ,240 ,241 ,242 ,243 ,244 ,245 ,246 ,247 ,248 ,249 ,250 ,251 ,252 ,253 ,254 ,255}
          };
        #elif MACHINE==2
          // sn_numa={{ 0 , 127 }, { 128 , 255 }};
          sn_numa = {
            {0 ,1 ,2 ,3 ,4 ,5 ,6 ,7 ,8 ,9 ,10 ,11 ,12 ,13 ,14 ,15 ,16 ,17 ,18 ,19 ,20 ,21 ,22 ,23 ,24 ,25 ,26 ,27 ,28 ,29 ,30 ,31 ,32 ,33 ,34 ,35 ,36 ,37 ,38 ,39 ,40 ,41 ,42 ,43 ,44 ,45 ,46 ,47 ,48 ,49 ,50 ,51 ,52 ,53 ,54 ,55 ,56 ,57 ,58 ,59 ,60 ,61 ,62 ,63 ,64 ,65 ,66 ,67 ,68 ,69 ,70 ,71 ,72 ,73 ,74 ,75 ,76 ,77 ,78 ,79 ,80 ,81 ,82 ,83 ,84 ,85 ,86 ,87 ,88 ,89 ,90 ,91 ,92 ,93 ,94 ,95 ,96 ,97 ,98 ,99 ,100 ,101 ,102 ,103 ,104 ,105 ,106 ,107 ,108 ,109 ,110 ,111 ,112 ,113 ,114 ,115 ,116 ,117 ,118 ,119 ,120 ,121 ,122 ,123 ,124 ,125 ,126 ,127}, 
            {128 ,129 ,130 ,131 ,132 ,133 ,134 ,135 ,136 ,137 ,138 ,139 ,140 ,141 ,142 ,143 ,144 ,145 ,146 ,147 ,148 ,149 ,150 ,151 ,152 ,153 ,154 ,155 ,156 ,157 ,158 ,159 ,160 ,161 ,162 ,163 ,164 ,165 ,166 ,167 ,168 ,169 ,170 ,171 ,172 ,173 ,174 ,175 ,176 ,177 ,178 ,179 ,180 ,181 ,182 ,183 ,184 ,185 ,186 ,187 ,188 ,189 ,190 ,191 ,192 ,193 ,194 ,195 ,196 ,197 ,198 ,199 ,200 ,201 ,202 ,203 ,204 ,205 ,206 ,207 ,208 ,209 ,210 ,211 ,212 ,213 ,214 ,215 ,216 ,217 ,218 ,219 ,220 ,221 ,222 ,223 ,224 ,225 ,226 ,227 ,228 ,229 ,230 ,231 ,232 ,233 ,234 ,235 ,236 ,237 ,238 ,239 ,240 ,241 ,242 ,243 ,244 ,245 ,246 ,247 ,248 ,249 ,250 ,251 ,252 ,253 ,254 ,255}
          };
        #elif MACHINE==3
          // sn_numa={{ 0 , 31 }, { 32 , 63 }, { 64 , 95 }, { 96 , 127 }, { 128 , 159 }, { 160 , 191 }, { 192 , 223 }, { 224 , 255 }};
          sn_numa ={
            {0 ,1 ,2 ,3 ,4 ,5 ,6 ,7 ,8 ,9 ,10 ,11 ,12 ,13 ,14 ,15 ,16 ,17 ,18 ,19 ,20 ,21 ,22 ,23 ,24 ,25 ,26 ,27 ,28 ,29 ,30 ,31}, 
            {32 ,33 ,34 ,35 ,36 ,37 ,38 ,39 ,40 ,41 ,42 ,43 ,44 ,45 ,46 ,47 ,48 ,49 ,50 ,51 ,52 ,53 ,54 ,55 ,56 ,57 ,58 ,59 ,60 ,61 ,62 ,63}, 
            {64 ,65 ,66 ,67 ,68 ,69 ,70 ,71 ,72 ,73 ,74 ,75 ,76 ,77 ,78 ,79 ,80 ,81 ,82 ,83 ,84 ,85 ,86 ,87 ,88 ,89 ,90 ,91 ,92 ,93 ,94 ,95}, 
            {96 ,97 ,98 ,99 ,100 ,101 ,102 ,103 ,104 ,105 ,106 ,107 ,108 ,109 ,110 ,111 ,112 ,113 ,114 ,115 ,116 ,117 ,118 ,119 ,120 ,121 ,122 ,123 ,124 ,125 ,126 ,127}, 
            {128 ,129 ,130 ,131 ,132 ,133 ,134 ,135 ,136 ,137 ,138 ,139 ,140 ,141 ,142 ,143 ,144 ,145 ,146 ,147 ,148 ,149 ,150 ,151 ,152 ,153 ,154 ,155 ,156 ,157 ,158 ,159}, 
            {160 ,161 ,162 ,163 ,164 ,165 ,166 ,167 ,168 ,169 ,170 ,171 ,172 ,173 ,174 ,175 ,176 ,177 ,178 ,179 ,180 ,181 ,182 ,183 ,184 ,185 ,186 ,187 ,188 ,189 ,190 ,191}, 
            {192 ,193 ,194 ,195 ,196 ,197 ,198 ,199 ,200 ,201 ,202 ,203 ,204 ,205 ,206 ,207 ,208 ,209 ,210 ,211 ,212 ,213 ,214 ,215 ,216 ,217 ,218 ,219 ,220 ,221 ,222 ,223}, 
            {224 ,225 ,226 ,227 ,228 ,229 ,230 ,231 ,232 ,233 ,234 ,235 ,236 ,237 ,238 ,239 ,240 ,241 ,242 ,243 ,244 ,245 ,246 ,247 ,248 ,249 ,250 ,251 ,252 ,253 ,254 ,255}
          };
        #elif MACHINE==4
          sn_numa={{
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
          }};
        #elif MACHINE==5
          // sn_numa = {{ 0 , 63 }, { 64 , 127 }, { 128 , 191 }, { 192 , 255 }};
          sn_numa = {
            {0 ,1 ,2 ,3 ,4 ,5 ,6 ,7 ,8 ,9 ,10 ,11 ,12 ,13 ,14 ,15 ,16 ,17 ,18 ,19 ,20 ,21 ,22 ,23 ,24 ,25 ,26 ,27 ,28 ,29 ,30 ,31 ,32 ,33 ,34 ,35 ,36 ,37 ,38 ,39 ,40 ,41 ,42 ,43 ,44 ,45 ,46 ,47 ,48 ,49 ,50 ,51 ,52 ,53 ,54 ,55 ,56 ,57 ,58 ,59 ,60 ,61 ,62 ,63}, 
            {64 ,65 ,66 ,67 ,68 ,69 ,70 ,71 ,72 ,73 ,74 ,75 ,76 ,77 ,78 ,79 ,80 ,81 ,82 ,83 ,84 ,85 ,86 ,87 ,88 ,89 ,90 ,91 ,92 ,93 ,94 ,95 ,96 ,97 ,98 ,99 ,100 ,101 ,102 ,103 ,104 ,105 ,106 ,107 ,108 ,109 ,110 ,111 ,112 ,113 ,114 ,115 ,116 ,117 ,118 ,119 ,120 ,121 ,122 ,123 ,124 ,125 ,126 ,127}, 
            {128 ,129 ,130 ,131 ,132 ,133 ,134 ,135 ,136 ,137 ,138 ,139 ,140 ,141 ,142 ,143 ,144 ,145 ,146 ,147 ,148 ,149 ,150 ,151 ,152 ,153 ,154 ,155 ,156 ,157 ,158 ,159 ,160 ,161 ,162 ,163 ,164 ,165 ,166 ,167 ,168 ,169 ,170 ,171 ,172 ,173 ,174 ,175 ,176 ,177 ,178 ,179 ,180 ,181 ,182 ,183 ,184 ,185 ,186 ,187 ,188 ,189 ,190 ,191}, 
            {192 ,193 ,194 ,195 ,196 ,197 ,198 ,199 ,200 ,201 ,202 ,203 ,204 ,205 ,206 ,207 ,208 ,209 ,210 ,211 ,212 ,213 ,214 ,215 ,216 ,217 ,218 ,219 ,220 ,221 ,222 ,223 ,224 ,225 ,226 ,227 ,228 ,229 ,230 ,231 ,232 ,233 ,234 ,235 ,236 ,237 ,238 ,239 ,240 ,241 ,242 ,243 ,244 ,245 ,246 ,247 ,248 ,249 ,250 ,251 ,252 ,253 ,254 ,255}
          }; 
        #endif 
        
        // std::uniform_int_distribution<int> dqt(0, valid_gcells.size()-1);  // you choose the numa node 
        // int choose_numa = dqt(gen);
        int choose_numa = gm->glbGridCell[valid_gcells[0]].idNUMA;
        valid_gcells=sn_numa[choose_numa];
        
      }


      // Push the query to the correct worker thread's job queue
      std::mt19937 genInts(rd());
      std::uniform_int_distribution<int> dq(0, valid_gcells.size()-1);
      
      int insert_tid = dq(genInts);
      int glbGridCellInsert = valid_gcells[insert_tid];
      query.aGrid = glbGridCellInsert;
      // // -------------------------------------------------------------------------------------
      // Update the query view of each cell
      gm->glbGridCell[glbGridCellInsert].qType[query.qStamp] += 1;
      gm->freqQueryDistPushed[glbGridCellInsert]++;
      gm->freqQueryDistCompleted[glbGridCellInsert]++;
        
      int cpuid = gm->glbGridCell[glbGridCellInsert].idCPU;
      glb_worker_thrds[cpuid].jobs.push(query);
      

      // int cpuid_idx = dq(genInt);
      // int cpuid = this->worker_cpuids[cpuid_idx];
      // glb_worker_thrds[cpuid].jobs.push(query);
      // -------------------------------------------------------------------------------------
      
      }
            
    });
  }
}

}  //namespace tp
} // namespace erebus
