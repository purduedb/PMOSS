#include "TPM.hpp"
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
      
      #if PROFILE==1
      PerfEvent e;
      int cnt = 0;  
      #endif
      
      while (1) {
        // Check for pause signal
        {
          std::unique_lock<std::mutex> lock(glb_worker_thrds[worker_cpuids[i]].pause_mutex);
          while (glb_worker_thrds[worker_cpuids[i]].paused) {
            // Wait for resume signal
            glb_worker_thrds[worker_cpuids[i]].pause_cv.wait(lock);
          }
        }

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
          
          #if PROFILE==1
          if (cnt == 0) e.startCounters();
          #endif

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
            else if(rec_pop.op == ycsbc::Operation::MIGRATE){
              result = this->gm->idx_btree->migrate_v1_(static_cast<uint64_t>(rec_pop.left_), int(rec_pop.right_), int(rec_pop.bottom_));
            }
            else{
              cout << "ycsb operation does not match" << endl;
            }
           
          #endif
          
          #if PROFILE==1
          cnt +=1;
          if (cnt == PERF_STAT_COLLECTION_INTERVAL){
            e.stopCounters();
            cnt=0;
            PerfCounter perf_counter;
            for(auto j=0; j < e.events.size(); j++){
              if (isnan(e.events[j].readCounter()) || isinf(e.events[j].readCounter())) 
                perf_counter.raw_counter_values[j] = 0;
              else perf_counter.raw_counter_values[j] = 
                e.events[j].readCounter();
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

// -------------------------------------------------------------------------------------
// Inference Server Communication (Gilbreth Server)
// -------------------------------------------------------------------------------------


bool TPManager::query_gilbreth_server(const TPManager::InferenceRequest& request) {
    // Before this the code should send the files that it has accumulated so far 
    // SSH into yrayhan@gilbreth.rcac.purdue.edu and run inference with GPU
    const std::string GILBRETH_USER = "yrayhan";
    const std::string GILBRETH_HOST = "gilbreth.rcac.purdue.edu";
    const std::string GILBRETH_INFERENCE_DIR = "/home/yrayhan/works/L-PMOSS";
    
    std::cout << "[Gilbreth] Connecting to Gilbreth and requesting GPU..." << std::endl;

    // Step 1: SSH into Gilbreth
    // Step 2: Request GPU using srun with specified resources (may queue)
    // Step 3: Navigate to L-PMOSS directory and execute run_dt.sh
    // Step 4: The script will generate output file c_<config>_256.txt on Gilbreth
    
    // Build the remote command that will be executed on Gilbreth
    // Pass workload and config as parameters to run_dt.sh
    // Note: Remove --pty for non-interactive commands
    // Source .bashrc to load module system and conda
    std::stringstream ssh_cmd;
    ssh_cmd << "ssh -o StrictHostKeyChecking=no " << GILBRETH_USER << "@" << GILBRETH_HOST << " '"
            << "srun -A csml -N 1 -p a30 --ntasks=1 --cpus-per-task=8 --mem=128G "
            << "--gres=gpu:1 --time=01:00:00 "
            << "/bin/bash -l -c \"cd " << GILBRETH_INFERENCE_DIR << " && ./infer_from_bigdata.sh " 
            << request.required_workload << " " << request.required_config << "\""
            << "'";

    std::cout << "[Gilbreth] Executing command with workload=" << request.required_workload 
              << ", config=" << request.required_config << std::endl;
    std::cout << "[Gilbreth] Full command: " << ssh_cmd.str() << std::endl;
    std::cout << "[Gilbreth] Note: This may queue waiting for GPU availability..." << std::endl;

    // Execute the SSH command (this will wait for GPU allocation and script completion)
    int ssh_result = system(ssh_cmd.str().c_str());
    if (ssh_result != 0) {
        std::cerr << "[Gilbreth] ERROR: SSH command failed with exit code " << ssh_result << std::endl;
        return -1;
    }

    std::cout << "[Gilbreth] Inference completed successfully" << std::endl;

    // Step 5: Copy the generated file from Gilbreth to local machine
    // File location on Gilbreth: /home/yrayhan/works/L-PMOSS/pmoss_machine_configs/intel_skx_4s_8n/[workload]/c_[config]_256.txt
    std::string remote_file_path = GILBRETH_INFERENCE_DIR + "/pmoss_machine_configs/intel_skx_4s_8n/" 
                                    + std::to_string(request.required_workload) + "/c_" 
                                    + std::to_string(request.required_config) + "_256.txt";
    std::string local_dir ="/homes/yrayhan/works/PMOSS/src/pmoss_machine_configs/intel_skx_4s_8n/" + std::to_string(request.required_workload);
    std::string local_file = local_dir + "/c_" + std::to_string(request.required_config) + "_256.txt";

    // Copy file from Gilbreth to local machine
    std::stringstream scp_cmd;
    scp_cmd << "scp -o StrictHostKeyChecking=no "
            << GILBRETH_USER << "@" << GILBRETH_HOST << ":"
            << remote_file_path << " "
            << local_file;

    std::cout << "[Gilbreth] Copying result file from: " << remote_file_path << std::endl;
    std::cout << "[Gilbreth] To local path: " << local_file << std::endl;

    int scp_result = system(scp_cmd.str().c_str());
    if (scp_result != 0) {
        std::cerr << "[Gilbreth] ERROR: Failed to copy result file (exit code: " << scp_result << ")" << std::endl;
        return -1;
    }

    std::cout << "[Gilbreth] File successfully copied to local machine" << std::endl;

    return 1;
}

// -------------------------------------------------------------------------------------

void TPManager::init_megamind_threads(int next_config, int next_workload, string next_config_path, int round){
  // Spawn a temporary detached thread for one-time delayed migration
  std::thread migration_thread([this, next_config, next_workload, next_config_path, round]() {
    CPUID migration_cpu = megamind_cpuids[0]; // Use first megamind CPU
    erebus::utils::PinThisThread(migration_cpu);
    // Delay to get the ncore sweeper and sys sweeper threads warmed up
    // const int WARMUP_DELAY = 0; // 70 seconds warmup
    // std::this_thread::sleep_for(std::chrono::milliseconds(WARMUP_DELAY));
    // Then send the contents 
    // this->pause_all_ncoresweepers();
    // this->dump_ncoresweeper_threads(round);
    // this->resume_all_ncoresweepers();
    // // an ssh command to send data to gilbreth server:rsync -aP /scratch/gilbreth/yrayhan/kbs/intel/skx_4s_8n/kb_b_dynam/ /homes/yrayhan/works/PMOSS/kb_bs_dynam/  
    // std::string ssh_cmd = "rsync -aP /scratch/gilbreth/yrayhan/kbs/intel/skx_4s_8n/kb_b_dynam/ /homes/yrayhan/works/PMOSS/kb_bs_dynam/";
    // int ssh_result = system(ssh_cmd.c_str());
    // if (ssh_result != 0) {
    //     std::cerr << "[Gilbreth] ERROR: SSH command failed with exit code " << ssh_result << std::endl;
    //     return -1;
    // }
    // const int SEND_DELAY = 0;
    // Sleep to simulate inference delay
    const int INFERENCE_DELAY = 70000; // 70 seconds to simulate inference delay
    std::this_thread::sleep_for(std::chrono::milliseconds(INFERENCE_DELAY));
    // InferenceRequest request;
    // request.required_config = next_config;
    // request.required_workload = next_workload;
    // bool response = query_gilbreth_server(request);
    // if(!response) assert(false);

    auto migration_start = std::chrono::high_resolution_clock::now();
    this->pause_all_routers();
    this->gm->reload_configuration(next_config_path);
    this->gm->config = next_config;
    this->resume_all_routers();
    
    // Start timing the migration
    #if SHARED_MIGRATION == 1
      for(size_t i = 0; i < MAX_GRID_CELL; i++){
        double lx = this->gm->glbGridCell[i].lx;
        int numa_id = this->gm->glbGridCell[i].idNUMA;
        int prev_cpu = this->gm->glbGridCell[i].prev_idCPU;
        int cpu_id = this->gm->glbGridCell[i].idCPU;
        Rectangle query;
        query.left_ = lx;
        query.right_= this->gm->DataDist[i];
        query.bottom_ = numa_id;
        query.op = ycsbc::Operation::MIGRATE;
        // Assign priority based on grid cell index for staggered execution
        // First grid cells get highest priority, later ones get lower priority
        query.qStamp = std::numeric_limits<int>::max() - i;
        query.aGrid = i;
        this->glb_worker_thrds[cpu_id].jobs.push(query);
        if (i % 32 == 0)
          std::this_thread::sleep_for(std::chrono::milliseconds(100));  
      }  
    #else
    // Pause all the router threads and worker threads
    this->pause_all_workers();
    // this->gm->enforce_scheduling();
    this->gm->enforce_scheduling_mt();
    this->resume_all_workers();
    #endif
    // Moved the routers to here 
    // this->resume_all_routers();
    
    // Calculate migration time
    auto migration_end = std::chrono::high_resolution_clock::now();
    auto migration_duration = std::chrono::duration_cast<std::chrono::milliseconds>(migration_end - migration_start);

    std::cout << "Megamind: Page migration completed in " << migration_duration.count() << "ms. Thread exiting." << std::endl;
  });

  // Detach the thread so it cleans up automatically when done
  migration_thread.detach();
}

void TPManager::init_syssweeper_threads(){
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

        // Check if thread should pause (for reconfiguration)
        {
          std::unique_lock<std::mutex> lock(glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].pause_mutex);
          if (glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].paused) {
            glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].pause_cv.wait(lock, [this, i]() {
              return !glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].paused;
            });
          }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10000));  //  60000
        
        // First, push the token to the worker cpus to get the DataView
        #if PROFILE==1
        PerfCounter perf_counter;
        perf_counter.qType = SYNC_TOKEN;
        for (auto[itr, rangeEnd] = this->gm->NUMAToWorkerCPUs.equal_range(numaID); itr != rangeEnd; ++itr)
        {
          int wkCPUID = itr->second;
          // cout << itr->first<< '\t' << itr->second << '\n';
          glb_worker_thrds[wkCPUID].perf_stats.push(perf_counter);
        }
        
        // Then, push the token to the system_sweeper cpu to get the System View (MemChannel)
        if (i == 0){
          IntelPCMCounter iPCMCnt;
          iPCMCnt.qType = SYNC_TOKEN;
          glb_sys_sweeper_thrds[sys_sweeper_cpuids[0]].pcmCounters.push(iPCMCnt);
        }
        #endif 
        
        // Take a snapshot of the DataView from the  threads
        const int nQCounterCline = PERF_EVENT_CNT/8 + PERF_EVENT_CNT%8;
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
                
                #if SIMD == 1
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
                #else
                ddSnap.rawCntSamples[pc.gIdx] += PERF_STAT_COLLECTION_INTERVAL; 
                for(auto ex = 0; ex < PERF_EVENT_CNT; ex++){
                  ddSnap.rawQCounter[pc.gIdx][ex] += (pc.raw_counter_values[ex] / pc.raw_counter_values[1])*1000;
                }
                ddSnap.rawQCounter[pc.gIdx][1] = pc.raw_counter_values[1];
                #endif

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
        // Take a snapshot of the System View (Memory Channel View)
        #if PROFILE == 1
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
                    DRAMResUsageSnap = iPCMCnt;
                }
                else
                    break;
            }
            glb_ncore_sweeper_thrds[ncore_sweeper_cpuids[i]].DRAMResUsageReel.push_back(DRAMResUsageSnap);
        }
        #endif
        
      }
    });
  }
}

void TPManager::dump_ncoresweeper_threads(int round){
  cout << "==========================DUMPING Core Sweeper Threads=======================" << endl;
  size_t num_samples = 0;
  for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) {
  string dirName;
  #if PROFILE ==1
  dirName = std::string(PROJECT_SOURCE_DIR);
  #if STORAGE == 0
      dirName += "/kb_r__/" + std::to_string(key);
  #elif STORAGE == 1
      dirName += "/kb_quad/" + std::to_string(key);
  #elif STORAGE == 2
      // dirName += "/kb_b__/" + std::to_string(key);  // This is for testing purpose 
      // dirName += "/kb_bs__/" + std::to_string(key);
      dirName += "/kb_bs_dynam/" + std::to_string(key);
      // dirName += "/kb_bs_profile/" + std::to_string(key);
      // dirName += "/kb_bs_4s_4n/" + std::to_string(key);
  #endif
  #elif PROFILE == 0
  dirName = std::string(PROJECT_SOURCE_DIR);
  #if STORAGE == 0
      dirName += "/kb_r__/" + std::to_string(key);
  #elif STORAGE == 1
      dirName += "/kb_quad/" + std::to_string(key);
  #elif STORAGE == 2
      dirName += "/kb_bs_profile/" + std::to_string(key);
      // dirName += "/kb_bs_4s_4n/" + std::to_string(key);
  #endif
  #endif
  
  mkdir(dirName.c_str(), 0777);
  cout << dirName << endl;

  cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;
  std::vector<DataDistSnap> localDataDistReel;
  std::vector<QueryExecSnap> localQueryExecReel;
  #if PROFILE == 1
  std::vector<IntelPCMCounter> localDRAMResUsageReel;
  localDRAMResUsageReel.swap(glb_ncore_sweeper_thrds[key].DRAMResUsageReel);
  #endif
  localDataDistReel.swap(glb_ncore_sweeper_thrds[key].dataDistReel);
  localQueryExecReel.swap(glb_ncore_sweeper_thrds[key].queryExecReel);
  
  num_samples = localQueryExecReel.size();
  
  cout << "DEBUG: dump_sample_counter = " << this->dump_sample_counter << ", num_samples = " << num_samples << endl;

        // -------------------------------------------------------------------------------------
    #if PROFILE == 1
    ofstream memChannelView(dirName + "/mem-channel_view.txt", std::ifstream::app);
    for(size_t i = 0; i < localDRAMResUsageReel.size(); i++){
        int tReel = this->dump_sample_counter + i;
        memChannelView << this->gm->config << " ";
        memChannelView << tReel << " ";
        memChannelView << this->gm->wkload << " ";
        memChannelView << this->gm->iam << " ";
        memChannelView << round << " ";
        /**
         * TODO: Have a global config header file that saves the value of 
         * global hw params
         * 6 definitely needs to be replaced with such param
         * It should not be numa_num_configured nodes
        */
        // Dump Read Socket Channel
        for (auto sc = 0; sc < 4; sc++){
            for(auto ch = 0; ch < 6; ch++){
                memChannelView <<  localDRAMResUsageReel[i].sysParams.iMC_Rd_socket_chan[sc][ch] << " ";
            }
        }
        // Dump Write Socket Channel
        for (auto sc = 0; sc < 4; sc++){
            for(auto ch = 0; ch < 6; ch++){
                memChannelView << localDRAMResUsageReel[i].sysParams.iMC_Wr_socket_chan[sc][ch] << " ";
            }
        }
        // // Dump Write Socket Channel
        for (auto sc = 0; sc < 4; sc++){
            for(auto ul = 0; ul < 3; ul++){
                memChannelView << localDRAMResUsageReel[i].upi_incoming[sc][ul] << " ";
            }
        }
        // Dump Write Socket Channel
        for (auto sc = 0; sc < 4; sc++){
            for(auto ul = 0; ul < 3; ul++){
                memChannelView << localDRAMResUsageReel[i].upi_outgoing[sc][ul] << " ";
            }
        }
        memChannelView << endl;
        
        // Clear this ith reel or do something so that it does not get bloated
        // glb_ncore_sweeper_thrds[key].DRAMResUsageReel[i].clear();
    }
    #endif
    // -------------------------------------------------------------------------------------
    ofstream dataView(dirName + "/data_view.txt", std::ifstream::app);
    const int nQCounterCline = PERF_EVENT_CNT/8 + PERF_EVENT_CNT%8;
    const int scalarDumpSize = MAX_GRID_CELL * nQCounterCline * 8;
    
    alignas(64) double dataViewScalarDump[scalarDumpSize] = {};
        
    for(size_t i = 0; i < localDataDistReel.size(); i++){
        int tReel = this->dump_sample_counter + i;
        DataDistSnap dd = localDataDistReel[i];

        dataView << this->gm->config  << " ";
        dataView << tReel << " ";
        dataView << this->gm->wkload << " ";
        dataView << this->gm->iam << " ";
        dataView << round << " ";

        #if SIMD == 1
        // Load the SIMD values in a memory address
        for (auto g = 0; g < MAX_GRID_CELL; g++){
            for (auto cLine = 0; cLine < nQCounterCline; cLine++){
                _mm512_store_pd(dataViewScalarDump + (g*nQCounterCline*8)+(cLine*8), dd.rawQCounter[g][cLine]);
            }     
        }
        #else
        for (auto g = 0; g < MAX_GRID_CELL; g++){
          memcpy(dataViewScalarDump+g*PERF_EVENT_CNT, dd.rawQCounter[g], sizeof(dd.rawQCounter[g]));
        }
        #endif
        
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
    // ofstream queryView(dirName + "/query_view.txt", std::ifstream::app);
    // for(size_t i = 0; i < glb_ncore_sweeper_thrds[key].queryViewReel.size(); i++){
    //     int tReel = i;
    //     queryView << this->gm->config  << " ";
    //     queryView << tReel << " ";
    //     queryView << this->gm->wkload << " ";
    //     queryView << this->gm->iam << " ";
    //     for(auto aSize1 = 0; aSize1 < MAX_GRID_CELL; aSize1++){
    //         for(auto aSize2 = 0; aSize2 < MAX_GRID_CELL; aSize2++){
    //             queryView << glb_ncore_sweeper_thrds[key].queryViewReel[i].corrQueryReel[aSize1][aSize2] << " ";
    //         }
    //     }
    //     queryView << endl;
    // }

    // -------------------------------------------------------------------------------------
    ofstream queryExecView(dirName + "/query-exec_view.txt", std::ifstream::app);
    for(size_t i = 0; i < localQueryExecReel.size(); i++){
        int tReel = this->dump_sample_counter + i;
        queryExecView << this->gm->config  << " ";
        queryExecView << tReel << " ";
        queryExecView << this->gm->wkload  << " ";
        queryExecView << this->gm->iam  << " ";
        queryExecView << round  << " ";
        for(auto aSize1 = 0; aSize1 < MAX_GRID_CELL; aSize1++){
            queryExecView << localQueryExecReel[i].qExecutedMice[aSize1] << " ";
            
        }
        queryExecView << endl;
    }

    // -------------------------------------------------------------------------------------

    cout << "==========================Finished dumping NCore Sweeper Thread =====> " << key <<  endl;
    cout << "-------------------------------------------------------------------------------------"  << endl;
}    
    this->dump_sample_counter += num_samples;
    cout << "==================================================================" << endl;

}

void TPManager::dump_ncoresweeper_threads_v2(){
  cout << "==========================DUMPING Core Sweeper Threads V2=======================" << endl;
  for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) {
    string dirName;
    #if PROFILE ==1
    dirName = std::string(PROJECT_SOURCE_DIR);
    #if STORAGE == 0
        dirName += "/kb_r__/" + std::to_string(key);
    #elif STORAGE == 1
        dirName += "/kb_quad/" + std::to_string(key);
    #elif STORAGE == 2
        dirName += "/kb_bs_dynam/" + std::to_string(key);
    #endif
    #elif PROFILE == 0
    dirName = std::string(PROJECT_SOURCE_DIR);
    #if STORAGE == 0
        dirName += "/kb_r__/" + std::to_string(key);
    #elif STORAGE == 1
        dirName += "/kb_quad/" + std::to_string(key);
    #elif STORAGE == 2
        dirName += "/kb_bs_profile/" + std::to_string(key);
    #endif
    #endif

    mkdir(dirName.c_str(), 0777);
    cout << dirName << endl;
    cout << "==========================Started dumping NCore Sweeper Thread =====> " << key << endl;

    // Atomically swap vectors to local copies - clears global vectors without contention
    std::vector<DataDistSnap> localDataDistReel;
    std::vector<QueryExecSnap> localQueryExecReel;
    #if PROFILE == 1
    std::vector<IntelPCMCounter> localDRAMResUsageReel;
    localDRAMResUsageReel.swap(glb_ncore_sweeper_thrds[key].DRAMResUsageReel);
    #endif
    localDataDistReel.swap(glb_ncore_sweeper_thrds[key].dataDistReel);
    localQueryExecReel.swap(glb_ncore_sweeper_thrds[key].queryExecReel);

    // -------------------------------------------------------------------------------------
    #if PROFILE == 1
    ofstream memChannelView(dirName + "/mem-channel_view.txt", std::ifstream::app);
    for(size_t i = 0; i < localDRAMResUsageReel.size(); i++){
        int tReel = i;
        memChannelView << this->gm->config << " ";
        memChannelView << tReel << " ";
        memChannelView << this->gm->wkload << " ";
        memChannelView << this->gm->iam << " ";

        // Dump Read Socket Channel
        for (auto sc = 0; sc < 4; sc++){
            for(auto ch = 0; ch < 6; ch++){
                memChannelView << localDRAMResUsageReel[i].sysParams.iMC_Rd_socket_chan[sc][ch] << " ";
            }
        }
        // Dump Write Socket Channel
        for (auto sc = 0; sc < 4; sc++){
            for(auto ch = 0; ch < 6; ch++){
                memChannelView << localDRAMResUsageReel[i].sysParams.iMC_Wr_socket_chan[sc][ch] << " ";
            }
        }
        // Dump UPI incoming
        for (auto sc = 0; sc < 4; sc++){
            for(auto ul = 0; ul < 3; ul++){
                memChannelView << localDRAMResUsageReel[i].upi_incoming[sc][ul] << " ";
            }
        }
        // Dump UPI outgoing
        for (auto sc = 0; sc < 4; sc++){
            for(auto ul = 0; ul < 3; ul++){
                memChannelView << localDRAMResUsageReel[i].upi_outgoing[sc][ul] << " ";
            }
        }
        memChannelView << endl;
    }
    memChannelView.close();
    #endif

    // -------------------------------------------------------------------------------------
    ofstream dataView(dirName + "/data_view.txt", std::ifstream::app);
    const int nQCounterCline = PERF_EVENT_CNT/8 + PERF_EVENT_CNT%8;
    const int scalarDumpSize = MAX_GRID_CELL * nQCounterCline * 8;

    alignas(64) double dataViewScalarDump[scalarDumpSize] = {};

    for(size_t i = 0; i < localDataDistReel.size(); i++){
        int tReel = i;
        DataDistSnap dd = localDataDistReel[i];

        dataView << this->gm->config  << " ";
        dataView << tReel << " ";
        dataView << this->gm->wkload << " ";
        dataView << this->gm->iam << " ";

        #if SIMD == 1
        // Load the SIMD values in a memory address
        for (auto g = 0; g < MAX_GRID_CELL; g++){
            for (auto cLine = 0; cLine < nQCounterCline; cLine++){
                _mm512_store_pd(dataViewScalarDump + (g*nQCounterCline*8)+(cLine*8), dd.rawQCounter[g][cLine]);
            }
        }
        #else
        for (auto g = 0; g < MAX_GRID_CELL; g++){
          memcpy(dataViewScalarDump+g*PERF_EVENT_CNT, dd.rawQCounter[g], sizeof(dd.rawQCounter[g]));
        }
        #endif

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
    dataView.close();

    // -------------------------------------------------------------------------------------
    ofstream queryExecView(dirName + "/query-exec_view.txt", std::ifstream::app);
    for(size_t i = 0; i < localQueryExecReel.size(); i++){
        int tReel = i;
        queryExecView << this->gm->config  << " ";
        queryExecView << tReel << " ";
        queryExecView << this->gm->wkload  << " ";
        queryExecView << this->gm->iam  << " ";
        for(auto aSize1 = 0; aSize1 < MAX_GRID_CELL; aSize1++){
            queryExecView << localQueryExecReel[i].qExecutedMice[aSize1] << " ";
        }
        queryExecView << endl;
    }
    queryExecView.close();

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

// -------------------------------------------------------------------------------------
// Dynamic Reconfiguration Methods
// -------------------------------------------------------------------------------------

void TPManager::pause_all_workers() {
    // Set pause flag for all worker threads
    for (const auto & [ key, value ] : glb_worker_thrds) {
        std::lock_guard<std::mutex> lock(glb_worker_thrds[key].pause_mutex);
        glb_worker_thrds[key].paused = true;
    }

    // Give workers time to reach pause point and drain in-flight queries
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void TPManager::resume_all_workers() {
    // Clear pause flag and wake all worker threads
    for (const auto & [ key, value ] : glb_worker_thrds) {
        {
            std::lock_guard<std::mutex> lock(glb_worker_thrds[key].pause_mutex);
            glb_worker_thrds[key].paused = false;
        }
        glb_worker_thrds[key].pause_cv.notify_one();
    }
}

bool TPManager::are_all_workers_idle() {
    // Check if all worker job queues are empty
    for (const auto & [ key, value ] : glb_worker_thrds) {
        if (glb_worker_thrds[key].jobs.size() > 0) {
            return false;
        }
    }
    return true;
}

void TPManager::clear_all_worker_queues() {
    // Clear all pending queries from worker thread queues
    // IMPORTANT: This should only be called AFTER pause_all_workers()
    size_t total_dropped = 0;

    // Extra safety: ensure all workers have reached pause point
    // This gives time for any in-flight queue operations to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (const auto & [ key, value ] : glb_worker_thrds) {
        // Acquire the pause_mutex to ensure the worker is truly paused
        // and not in the middle of accessing the queue
        std::lock_guard<std::mutex> lock(glb_worker_thrds[key].pause_mutex);

        // Verify worker is actually paused before clearing
        if (!glb_worker_thrds[key].paused) {
            std::cerr << "WARNING: Worker " << key << " not paused when clearing queue!" << std::endl;
            continue;
        }

        size_t queue_size = glb_worker_thrds[key].jobs.size();
        total_dropped += queue_size;

        // Clear the queue directly - much more efficient than popping each element
        // Safe to call because we hold the pause_mutex and worker is paused
        glb_worker_thrds[key].jobs.clear();
    }

    if (total_dropped > 0) {
        std::cout << "  Cleared " << total_dropped << " pending queries from worker queues" << std::endl;
    }
}

void TPManager::pause_all_routers() {
    // Set pause flag for all router threads
    for (const auto & [ key, value ] : glb_router_thrds) {
        std::lock_guard<std::mutex> lock(glb_router_thrds[key].pause_mutex);
        glb_router_thrds[key].paused = true;
    }

    // Give routers time to reach pause point
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void TPManager::resume_all_routers() {
    // Clear pause flag and wake all router threads
    for (const auto & [ key, value ] : glb_router_thrds) {
        {
            std::lock_guard<std::mutex> lock(glb_router_thrds[key].pause_mutex);
            glb_router_thrds[key].paused = false;
        }
        glb_router_thrds[key].pause_cv.notify_one();
    }
}

void TPManager::pause_all_ncoresweepers() {
    // Set pause flag for all node core sweeper threads
    for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) {
        std::lock_guard<std::mutex> lock(glb_ncore_sweeper_thrds[key].pause_mutex);
        glb_ncore_sweeper_thrds[key].paused = true;
    }

    // Give core sweepers time to reach pause point
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void TPManager::resume_all_ncoresweepers() {
    // Clear pause flag and wake all node core sweeper threads
    for (const auto & [ key, value ] : glb_ncore_sweeper_thrds) {
        {
            std::lock_guard<std::mutex> lock(glb_ncore_sweeper_thrds[key].pause_mutex);
            glb_ncore_sweeper_thrds[key].paused = false;
        }
        glb_ncore_sweeper_thrds[key].pause_cv.notify_one();
    }
}


void TPManager::initiate_workload_change(int new_workload) {
    std::cout << "Initiating workload change to: " << new_workload << std::endl;

    // Signal all router threads that workload change is pending
    for (const auto & [ key, value ] : glb_router_thrds) {
        glb_router_thrds[key].current_workload.store(new_workload);
        glb_router_thrds[key].workload_change_pending.store(true);
    }
    active_workload.store(new_workload);
}

void TPManager::wait_for_router_sync(int router_id, int new_workload) {
    std::unique_lock<std::mutex> lock(workload_change_mutex);

    // Increment ready counter
    int count = router_ready_count.fetch_add(1) + 1;
    std::cout << "Router " << router_id << " reached barrier (" << count << "/" << CURR_ROUTER_THREADS << ")" << std::endl;

    // Notify coordinator
    workload_change_cv.notify_one();

    // Wait until all routers are ready and global workload is updated
    workload_change_cv.wait(lock, [this, new_workload]() {
        return active_workload.load() == new_workload;
    });

    std::cout << "Router " << router_id << " proceeding with new workload" << std::endl;
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
    glb_router_thrds[router_cpuids[i]].th = std::thread([i, this, ds, wl, min_x, max_x, min_y, max_y, &init_keys, &values] () mutable {

    erebus::utils::PinThisThread(router_cpuids[i]);
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
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
    

    // btree experiments
    // b tree experiments: hotspot wkload
    std::array<normal_dist, 16> b_GX;
    std::uniform_int_distribution<int> dslength_uint64;  
    std::uniform_int_distribution<int> coin_toss_dist(0, 1);

    std::uniform_int_distribution<uint64_t> dx_uint64; 

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
      wl == SD_YCSB_WKLOADI || wl == SD_YCSB_WKLOADA1 || 
      wl == WIKI_WKLOADA || wl == WIKI_WKLOADC || wl == WIKI_WKLOADE || wl == WIKI_WKLOADI ||
      wl == WIKI_WKLOADH || wl == WIKI_WKLOADA1 || wl == WIKI_WKLOADA2 || wl == WIKI_WKLOADA3 ||
      wl == OSM_WKLOADA || wl == OSM_WKLOADC || wl == OSM_WKLOADE || wl == OSM_WKLOADH || wl == OSM_WKLOADA0 ||
      wl == SD_YCSB_WKLOADH1 || wl == SD_YCSB_WKLOADH2 || wl == SD_YCSB_WKLOADH3 || wl == SD_YCSB_WKLOADH4 || wl == SD_YCSB_WKLOADH5 ||
      wl == SD_YCSB_WKLOADA00 || wl == SD_YCSB_WKLOADA01 || wl == SD_YCSB_WKLOADC1 || wl == SD_YCSB_WKLOADH11 ||
      wl == SD_YCSB_WKLOADK || wl == SD_YCSB_WKLOADK2 || wl == SD_YCSB_WKLOADK3 || wl == SD_YCSB_WKLOADK4
    ){
      
      std::ifstream input;
      #if MACHINE==0
      std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/skx_4s_8n/";
      #elif MACHINE==1
      std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/ice_2s_2n/";
      #elif MACHINE==5
      std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/sb_4s_4n/";  // this should be nvidia
      #elif MACHINE==6
      std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/skx_4s_4n/";
      #elif MACHINE==2 || MACHINE == 7
      std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/epyc7543_2s_2n/";
      #elif MACHINE==3
      std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/epyc7543_2s_2n/";
      #endif

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
      else if (wl == SD_YCSB_WKLOADA1){
        wl_config += "ycsb_workloada1_" + to_string(router_cpuids[i]);
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
      else if (wl == SD_YCSB_WKLOADH11){
        wl_config += "ycsb_workloadh11";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADI){
        wl_config += "ycsb_workloadi";
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADK){
        wl_config += "ycsb_workloadk_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADK2){
        wl_config += "ycsb_workloadk2_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADK3){
        wl_config += "ycsb_workloadk3_" + to_string(router_cpuids[i]);
        input.open(wl_config);
      }
      else if (wl == SD_YCSB_WKLOADK4){
        wl_config += "ycsb_workloadk4_" + to_string(router_cpuids[i]);
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
    else if (wl == SD_YCSB_WKLOADX1){
      const int num_hspots = 8;
      std::vector<int> x_list = utils::linspace<int>(0, BTREE_INIT_LIMIT, num_hspots+1);
      int std_dev = 300000;
      
      for (int spIdx = 0; spIdx < num_hspots; spIdx++){
          int mean_x = int((x_list[spIdx] + x_list[spIdx+1])/2);
          b_GX[spIdx] = normal_dist{static_cast<double>(mean_x), static_cast<double>(std_dev)};
          
      }
      w = discrete_dist{0.12, 0.12, 0.12, 0.16, 0.12, 0.12, 0.12, 0.12};
      dslength_uint64 = std::uniform_int_distribution<> (1, 30000); //default ycsb value
      
    } 
    else if (wl == SD_YCSB_WKLOADX2){
      const int num_hspots = 8;
      std::vector<double> x_list = utils::linspace<double>(min_x, max_x, num_hspots+1);
      int std_dev = 36028796474878160;
      
      for (int spIdx = 0; spIdx < num_hspots; spIdx++){
          double mean_x = (x_list[spIdx] + x_list[spIdx+1])/2;
          b_GX[spIdx] = normal_dist{static_cast<double>(mean_x), static_cast<double>(std_dev)};
          
      }
      w = discrete_dist{0.12, 0.12, 0.12, 0.16, 0.12, 0.12, 0.12, 0.12};
      dslength_uint64 = std::uniform_int_distribution<> (1, 30000); //default ycsb value
      
    } 

    // -------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------
    // Initialize rate control tracking
    glb_router_thrds[router_cpuids[i]].second_start_time = std::chrono::steady_clock::now();
    glb_router_thrds[router_cpuids[i]].queries_this_second = 0;
    
    while(1) {
      // ==================================================================================
      // QUERY RATE CONTROL LOGIC
      // ==================================================================================
      // if (RATE_CONTROL_ENABLED and wl==SD_YCSB_WKLOADA) {  // Enable rate control only for specific workloads
      //   auto current_time = std::chrono::steady_clock::now();
      //   auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      //       current_time - glb_router_thrds[router_cpuids[i]].second_start_time);
        
      //   // If we've been running for more than 1 second, reset the counter
      //   if (elapsed.count() >= 1000) {
      //     glb_router_thrds[router_cpuids[i]].second_start_time = current_time;
      //     glb_router_thrds[router_cpuids[i]].queries_this_second = 0;
      //   }
        
      //   // If we've reached the query limit for this second, sleep until the next second
      //   if (glb_router_thrds[router_cpuids[i]].queries_this_second >= QUERIES_PER_SECOND) {
      //     auto time_to_sleep = std::chrono::milliseconds(1000) - elapsed;
      //     if (time_to_sleep.count() > 0) {
      //       std::this_thread::sleep_for(time_to_sleep);
      //     }
      //     // Reset for the next second
      //     glb_router_thrds[router_cpuids[i]].second_start_time = std::chrono::steady_clock::now();
      //     glb_router_thrds[router_cpuids[i]].queries_this_second = 0;
      //   }
      // }
      // ==================================================================================
      
      // We need to be able to handle the load factor by fixing the number of queries generated by 
      // each router thread. So, each router thread generates a fixed number of queries per time unit (e.g., second)
      

      // Check for pause signal (grid cell update support)
      {
        std::unique_lock<std::mutex> lock(glb_router_thrds[router_cpuids[i]].pause_mutex);
        while (glb_router_thrds[router_cpuids[i]].paused) {
          glb_router_thrds[router_cpuids[i]].pause_cv.wait(lock);
        }
      }

      // Check for workload change (dynamic reconfiguration support)
      if (glb_router_thrds[router_cpuids[i]].workload_change_pending.load()) {
        int new_wl = glb_router_thrds[router_cpuids[i]].current_workload.load();
        std::cout << "Router " << i << " detected workload change to " << new_wl << std::endl;
        wl = new_wl;

        if (wl == SD_YCSB_WKLOADA || wl == SD_YCSB_WKLOADC || wl == SD_YCSB_WKLOADE ||
            wl == SD_YCSB_WKLOADF || wl == SD_YCSB_WKLOADE1 || wl == SD_YCSB_WKLOADH ||
            wl == SD_YCSB_WKLOADI || wl == SD_YCSB_WKLOADA1 ||
            wl == WIKI_WKLOADA || wl == WIKI_WKLOADC || wl == WIKI_WKLOADE || wl == WIKI_WKLOADI ||
            wl == WIKI_WKLOADH || wl == WIKI_WKLOADA1 || wl == WIKI_WKLOADA2 || wl == WIKI_WKLOADA3 ||
            wl == OSM_WKLOADA || wl == OSM_WKLOADC || wl == OSM_WKLOADE || wl == OSM_WKLOADH || wl == OSM_WKLOADA0 ||
            wl == SD_YCSB_WKLOADH1 || wl == SD_YCSB_WKLOADH2 || wl == SD_YCSB_WKLOADH3 || wl == SD_YCSB_WKLOADH4 || wl == SD_YCSB_WKLOADH5 ||
            wl == SD_YCSB_WKLOADA00 || wl == SD_YCSB_WKLOADA01 || wl == SD_YCSB_WKLOADC1 || wl == SD_YCSB_WKLOADH11 ||
            wl == SD_YCSB_WKLOADK || wl == SD_YCSB_WKLOADK2 || wl == SD_YCSB_WKLOADK3 || wl == SD_YCSB_WKLOADK4) {

          // Reload YCSB workload configuration
          std::ifstream input;
          #if MACHINE==0
          std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/skx_4s_8n/";
          #elif MACHINE==1
          std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/ice_2s_2n/";
          #elif MACHINE==5
          std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/sb_4s_4n/";
          #elif MACHINE==6
          std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/skx_4s_4n/";
          #elif MACHINE==2 || MACHINE == 7
          std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/epyc7543_2s_2n/";
          #elif MACHINE==3
          std::string wl_config = std::string(PROJECT_SOURCE_DIR) + "/src/workloads/2s_8n/";
          #endif

          // Select appropriate workload file based on new_wl
          if (wl == SD_YCSB_WKLOADA) wl_config += "ycsb_workloada_" + to_string(router_cpuids[i]);
          else if (wl == SD_YCSB_WKLOADC) wl_config += "ycsb_workloadc";
          else if (wl == SD_YCSB_WKLOADE) wl_config += "ycsb_workloade_" + to_string(router_cpuids[i]);
          else if (wl == SD_YCSB_WKLOADH) wl_config += "ycsb_workloadh";
          else if (wl == SD_YCSB_WKLOADK2) wl_config += "ycsb_workloadk2_" + to_string(router_cpuids[i]);
          else {
            std::cerr << "Unsupported dynamic workload change to: " << wl << std::endl;
          }
          
          ycsbc::utils::Properties fresh_props;
          input.open(wl_config);
          if (input.is_open()) {
            fresh_props.Load(input);
            input.close();
            ycsb_wl.Init(fresh_props);
            std::cout << "Router " << i << " successfully reloaded YCSB workload config from: " << wl_config << std::endl;
            std::cout << "       Properties loaded: readproportion=" << fresh_props.GetProperty("readproportion", "N/A")
                      << ", scanproportion=" << fresh_props.GetProperty("scanproportion", "N/A") << std::endl;
          } else {
            std::cerr << "ERROR: Router " << i << " FAILED to open workload file: " << wl_config << std::endl;
            std::cerr << "       Workload change ABORTED - router will continue with previous workload!" << std::endl;
            std::cerr << "       This will cause SEVERE performance degradation!" << std::endl;
          }
        } else {
          std::cout << "Warning: Non-YCSB workload change may not be fully supported" << std::endl;
        }

        glb_router_thrds[router_cpuids[i]].workload_change_pending.store(false);
        std::cout << "Router " << i << " completed workload change" << std::endl;
      }

      if(!glb_router_thrds[router_cpuids[i]].running) {
          break;
      }

      // Query parameters 
      double lx, ly, hx, hy, length, width;
      Rectangle query;
      uint64_t tx_keys[3] = {0};

    if(wl == SD_YCSB_WKLOADA || wl == SD_YCSB_WKLOADC || wl == SD_YCSB_WKLOADE ||
      wl == SD_YCSB_WKLOADF || wl == SD_YCSB_WKLOADE1 || wl == SD_YCSB_WKLOADH || 
      wl == SD_YCSB_WKLOADI || wl == SD_YCSB_WKLOADA1 || 
      wl == WIKI_WKLOADA || wl == WIKI_WKLOADC || wl == WIKI_WKLOADE || wl == WIKI_WKLOADI || 
      wl == WIKI_WKLOADH || wl == WIKI_WKLOADA1 || wl == WIKI_WKLOADA2 || wl == WIKI_WKLOADA3 ||
      wl == OSM_WKLOADA || wl == OSM_WKLOADC || wl == OSM_WKLOADE || wl == OSM_WKLOADH || wl == OSM_WKLOADA0 ||
      wl == SD_YCSB_WKLOADH1 || wl == SD_YCSB_WKLOADH2 || wl == SD_YCSB_WKLOADH3 || wl == SD_YCSB_WKLOADH4 || wl == SD_YCSB_WKLOADH5 ||
      wl == SD_YCSB_WKLOADA00 || wl == SD_YCSB_WKLOADA01 || wl == SD_YCSB_WKLOADC1 || wl == SD_YCSB_WKLOADH11 ||
      wl == SD_YCSB_WKLOADK || wl == SD_YCSB_WKLOADK2 || wl == SD_YCSB_WKLOADK3 || wl == SD_YCSB_WKLOADK4
    ){
      ycsb_wl.DoTransaction(tx_keys);
      uint64_t value = -1;

      // Bounds check to prevent segfault when workload changes
      if (tx_keys[0] >= init_keys.size()) {
        // std::cerr << "ERROR: tx_keys[0]=" << tx_keys[0] << " exceeds init_keys.size()=" << init_keys.size() << std::endl;
        continue; // Skip this invalid query
      }

      lx = init_keys[tx_keys[0]]; // The key to insert/search/update
      length = tx_keys[1];  // in case of range scan
      if (tx_keys[2] == ycsbc::Operation::INSERT) value = values[tx_keys[0]];  // in case of
      query = Rectangle(lx, length, value, -1);
      query.op = tx_keys[2];
      // cout << tx_keys[0] << ' ' << tx_keys[2] << endl;
    }
    else {
      std::cerr << "Workload not supported in the current implementation." << std::endl;
    }

      // -------------------------------------------------------------------------------------
      // Check which grid the query belongs to 
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
          else continue;
        #endif 
            
      }
      
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
      

      if(this->gm->config == 506){
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
        #elif MACHINE==2 || MACHINE == 7 || MACHINE == 8
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
        #elif MACHINE==5 || MACHINE == 6
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
        std::mt19937 genInt(rd());
        std::uniform_int_distribution<int> dq(0, valid_gcells.size()-1); 
        int insert_tid = dq(genInt);
        int glbGridCellInsert = valid_gcells[insert_tid];
              
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
        // CRITICAL SECTION: Acquire shared read lock to read idCPU
        // This ensures we always read the correct CPU assignment even during reconfiguration
        int cpuid;
        {
            std::shared_lock<std::shared_mutex> lock(gm->config_mutex);
            cpuid = gm->glbGridCell[glbGridCellInsert].idCPU;
        }
        glb_worker_thrds[cpuid].jobs.push(query);
        // -------------------------------------------------------------------------------------
        // Increment query counter for rate control
        // if (RATE_CONTROL_ENABLED) {
        //   glb_router_thrds[router_cpuids[i]].queries_this_second++;
        // }
        // -------------------------------------------------------------------------------------
        // Use it as a throttling factor
        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }           
    });
  }
}

}  //namespace tp
} // namespace erebus
