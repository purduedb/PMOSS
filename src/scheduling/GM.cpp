#include "GM.hpp"
// -------------------------------------------------------------------------------------


namespace erebus
{
namespace dm
{
GridManager::GridManager(int config, int wkload, int iam, int xPar, int yPar, double minXSpace, double maxXSpace, double minYSpace, double maxYSpace){
	this->config = config;
    this->wkload = wkload;
    this->iam = iam;
    this->xPar = xPar;
	this->yPar = yPar;
	this->minXSpace = minXSpace;
	this->maxXSpace = maxXSpace;
	this->minYSpace = minYSpace;
	this->maxYSpace = maxYSpace;
    this->btree_key_count.store(BTREE_INIT_LIMIT);
#if MULTIDIM == 1
	this->nGridCells = this->xPar * this->yPar;
#else 
	this->nGridCells = this->xPar;
#endif 
	this->idx = nullptr;

	for(auto i = 0; i < MAX_GRID_CELL; i++) this->DataDist.push_back(0);
}


void GridManager::register_grid_cells(vector<CPUID> availCPUs){
    int nGridCellsPerThread = this->nGridCells / availCPUs.size() + 1; 

    std::vector<double> xList = utils::linspace<double>(this->minXSpace, this->maxXSpace, this->xPar+1);
    std::vector<double> yList = utils::linspace<double>(this->minYSpace, this->maxYSpace, this->yPar+1);
    double delX = xList[1] - xList[0];
    double delY = yList[1] - yList[0];
    
    // -------------------------------------------------------------------------------------
    // 1. #instruction, 2. #Accesses (Shadows Data)
    ifstream ifsLRCoeff1("./src/stamp_model/lr_coeff_ins.txt", std::ifstream::in);
    ifstream ifsLRCoeff2("./src/stamp_model/lr_coeff_acc.txt", std::ifstream::in);
    

    int trk_cid = 0;
    
    for(auto i = 0; i < this->xPar; i++){
        for (auto j = 0; j < this->yPar; j++){
            this->glbGridCell[trk_cid].cid = trk_cid;
            
            this->glbGridCell[trk_cid].lx = xList[i];
            this->glbGridCell[trk_cid].ly = yList[j];
            this->glbGridCell[trk_cid].hx = xList[i]+delX;
            this->glbGridCell[trk_cid].hy = yList[j]+delY;
            
            this->glbGridCell[trk_cid].idCPU = availCPUs[trk_cid/nGridCellsPerThread];
            this->glbGridCell[trk_cid].idNUMA = numa_node_of_cpu(availCPUs[trk_cid/nGridCellsPerThread]); 
            
            // -------------------------------------------------------------------------------------
            #if USE_MODEL
            for(auto pI = 0; pI < STAMP_LR_PARAM; pI++){
                ifsLRCoeff1 >> this->glbGridCell[trk_cid].lRegCoeff[0][pI];
                ifsLRCoeff2 >> this->glbGridCell[trk_cid].lRegCoeff[1][pI];
            }
            #endif

            trk_cid++; 
        }
    }
}

void GridManager::register_grid_cells(string configFile){
	ifstream ifs(configFile, std::ifstream::in);
	vector<NUMAID> numaConfig; 
	vector<CPUID> cpuConfig;
    
  for (int i = 0; i < nGridCells; i++) {
		NUMAID nID;
		ifs >> nID;
		numaConfig.push_back(nID);
		// cout << nID << " ";
	}
  
  for (int i = 0; i < nGridCells; i++) {
		CPUID cpuID;
		ifs >> cpuID;
		cpuConfig.push_back(cpuID);
    // cout << cpuID << " ";
	}
    
	std::vector<double> xList = utils::linspace<double>(this->minXSpace, this->maxXSpace, this->xPar+1);
	std::vector<double> yList = utils::linspace<double>(this->minYSpace, this->maxYSpace, this->yPar+1);
	double delX = xList[1] - xList[0];
	double delY = yList[1] - yList[0];
	
	// -------------------------------------------------------------------------------------
	
	int trk_cid = 0;
  

	for(auto i = 0; i < this->xPar; i++){
		for (auto j = 0; j < this->yPar; j++){

			this->glbGridCell[trk_cid].cid = trk_cid;
			
			this->glbGridCell[trk_cid].lx = xList[i];
			this->glbGridCell[trk_cid].ly = yList[j];
			this->glbGridCell[trk_cid].hx = xList[i]+delX;
			this->glbGridCell[trk_cid].hy = yList[j]+delY;
			
			
			this->glbGridCell[trk_cid].idNUMA = numaConfig[trk_cid];
			this->glbGridCell[trk_cid].idCPU = cpuConfig[trk_cid]; 
			this->glbGridCell[trk_cid].has_migrated = false;
			trk_cid++; 
		}
	}
  
  
}

void GridManager::enforce_scheduling(){
  auto start = std::chrono::high_resolution_clock::now();
  
  for(size_t i = 0; i < MAX_GRID_CELL; i++){
    // auto start1 = std::chrono::high_resolution_clock::now();
    double lx = this->glbGridCell[i].lx;
    double hx = this->glbGridCell[i].hx;
    double ly = this->glbGridCell[i].ly;
    double hy = this->glbGridCell[i].hy;
    int numa_id = this->glbGridCell[i].idNUMA;
    int prev_numa_id = this->glbGridCell[i].prev_idNUMA;
    if (numa_id == prev_numa_id) {
        continue; // Skip migration if already on the correct NUMA node
    }
    #if LINUX != 0
		#if STORAGE == 0
			MigrateNodes(this->idx, lx, hx, ly, hy, numa_id);    
		#elif STORAGE == 1
			MigrateNodesQuad(this->idx_quadtree, lx, hx, ly, hy, numa_id);    
		#elif STORAGE == 2
			// this->idx_btree->migrate_(lx, this->DataDist[i], numa_id);
            this->idx_btree->migrate_v1_(lx, this->DataDist[i], numa_id);
		#endif
	#endif
    // auto finish1 = std::chrono::high_resolution_clock::now();
    // std::chrono::duration<double> elapsed1 = finish1 - start1;
    // cout << "Checkpoint: SINGLE_MIGRATION_COMPLETED: " << elapsed1.count() << endl;
  }
  auto finish = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = finish - start;
  cout << "Checkpoint: INDEX_MIGRATION_COMPLETED: " << elapsed.count() << endl;
  
}


void GridManager::enforce_scheduling_mt(){
  auto start = std::chrono::high_resolution_clock::now();

  const int NUM_THREADS = 40;
  const int CELLS_PER_THREAD = MAX_GRID_CELL / NUM_THREADS; // 256 / 40 = 6 cells per thread

  std::vector<std::thread> threads;
  threads.reserve(NUM_THREADS);

  // Lambda function for each thread to process its assigned grid cells
  auto migrate_worker = [this](int start_idx, int end_idx) {
    for(int i = start_idx; i < end_idx; i++){
      double lx = this->glbGridCell[i].lx;
      double hx = this->glbGridCell[i].hx;
      double ly = this->glbGridCell[i].ly;
      double hy = this->glbGridCell[i].hy;
      int numa_id = this->glbGridCell[i].idNUMA;
      int prev_numa_id = this->glbGridCell[i].prev_idNUMA;
      if (numa_id == prev_numa_id) {
          continue; // Skip migration if already on the correct NUMA node
      }
            
      #if LINUX != 0
        #if STORAGE == 0
          MigrateNodes(this->idx, lx, hx, ly, hy, numa_id);
        #elif STORAGE == 1
          MigrateNodesQuad(this->idx_quadtree, lx, hx, ly, hy, numa_id);
        #elif STORAGE == 2
          // this->idx_btree->migrate_(lx, this->DataDist[i], numa_id);
          this->idx_btree->migrate_v1_(lx, this->DataDist[i], numa_id);
        #endif
      #endif
    }
  };

  // Launch threads, each handling a portion of the grid cells
  for(int t = 0; t < NUM_THREADS; t++){
    int start_idx = t * CELLS_PER_THREAD;
    int end_idx = (t == NUM_THREADS - 1) ? MAX_GRID_CELL : (t + 1) * CELLS_PER_THREAD;
    threads.emplace_back(migrate_worker, start_idx, end_idx);
  }

  // Wait for all threads to complete
  for(auto &thread : threads){
    thread.join();
  }

  auto finish = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = finish - start;
  cout << "Checkpoint: INDEX_MIGRATION_COMPLETED (MT): " << elapsed.count() << endl;

}




void GridManager::enforce_scheduling_batch(){
    auto start = std::chrono::high_resolution_clock::now();
    // We want to get the ids of the grid cells that have the same numa id
    std::unordered_map<int, std::vector<int>> numa_map;
    for(size_t i = 0; i < MAX_GRID_CELL; i++){
        numa_map[this->glbGridCell[i].idNUMA].push_back(i);
    }
    for(auto &pair : numa_map){
        int numa_id = pair.first;
        std::vector<int> &cell_ids = pair.second;
        std::vector<std::tuple<uint64_t, int>> bounds;
        for(auto &cid : cell_ids)
            bounds.push_back(std::make_tuple(
                this->glbGridCell[cid].lx,
                this->DataDist[cid]
            ));
        // Now we can migrate all nodes in these bounds to the same numa node
        this->idx_btree->migrate_batch(bounds, numa_id);
    }
  
    // auto finish1 = std::chrono::high_resolution_clock::now();
    // std::chrono::duration<double> elapsed1 = finish1 - start1;
    // cout << "Checkpoint: SINGLE_MIGRATION_COMPLETED: " << elapsed1.count() << endl;
  auto finish = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = finish - start;
  cout << "Checkpoint: INDEX_MIGRATION_COMPLETED: " << elapsed.count() << endl;
  
}


void GridManager::register_index(erebus::storage::rtree::RTree * idx)
{
    this->idx = idx;
}
void GridManager::register_index(erebus::storage::qtree::QuadTree * idx_quadtree)
{
    this->idx_quadtree = idx_quadtree;
}

void GridManager::register_index(erebus::storage::BTreeOLCIndex<keytype, keycomp> *idx_btree){
    this->idx_btree = idx_btree;
}

void GridManager::printGM(){
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "------------------------------------Grid Resources-----------------------------------" << endl;
    cout << "QueryDistribution" << endl;
    for(auto j = 0; j < this->yPar; j++){
        for (auto i = 0; i < this->xPar; i++){
            cout << "|\t" << "(" << 
                glbGridCell[this->yPar*i + j].idCPU <<
                ", " << 
                glbGridCell[this->yPar*i + j].idNUMA <<
            ")" << "\t|";
        }
        cout << endl;
    }
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------------------------------------------------------------" << endl;
}

void GridManager::printQueryDistPushed(){
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------QueryDistribution: Pushed-----------------------------" << endl;
    cout << "" << endl;

    for(auto j = 0; j < this->yPar; j++){
        for (auto i = 0; i < this->xPar; i++){
            cout << "|\t" <<  
                freqQueryDistPushed[this->yPar*i + j] <<
                "\t|";
        }
        cout << endl;
    }
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------------------------------------------------------------" << endl;
}
void GridManager::printQueryDistCompleted(){
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------QueryDistribution: Completed-----------------------------" << endl;
    cout << "" << endl;

    for(auto j = 0; j < this->yPar; j++){
        for (auto i = 0; i < this->xPar; i++){
            cout << "|\t" <<  
                freqQueryDistCompleted[this->yPar*i + j] <<
                "\t|";
        }
        cout << endl;
    }
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------------------------------------------------------------" << endl;
}
void GridManager::printQueryDistOstanding(){
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------QueryDistribution: Outstanding------------------------" << endl;
    cout << "" << endl;

    for(auto j = 0; j < this->yPar; j++){
        for (auto i = 0; i < this->xPar; i++){
            cout << "|\t" <<  
                freqQueryDistPushed[this->yPar*i + j] - freqQueryDistCompleted[this->yPar*i + j] <<
                "\t|";
        }
        cout << endl;
    }
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------------------------------------------------------------" << endl;
}

void GridManager::buildDataDistIdx(int access_method, std::vector<keytype> &init_keys){
    // Clear existing distribution
    std::fill(DataDist.begin(), DataDist.end(), 0);
    
    if (access_method == BTREE){
      const int NUM_THREADS = 56;
      const unsigned int KEYS_PER_THREAD = BTREE_INIT_LIMIT / NUM_THREADS;
      
      // Thread-local counters to avoid false sharing and contention
      std::vector<std::vector<int>> thread_local_dists(NUM_THREADS, std::vector<int>(nGridCells, 0));
      
      std::vector<std::thread> threads;
      threads.reserve(NUM_THREADS);
      
      auto count_worker = [&](int thread_id, unsigned int start_idx, unsigned int end_idx) {
        for(unsigned int i = start_idx; i < end_idx; i++){
          double lx = init_keys[i];
          
          for (auto gc = 0; gc < nGridCells; gc++){
            double glx = glbGridCell[gc].lx;
            double ghx = glbGridCell[gc].hx;
            
            if (lx <= ghx && lx >= glx) {
              thread_local_dists[thread_id][gc]++;
              break; // Key can only be in one grid cell
            }
          }
        }
      };
      
      // Launch threads
      for(int t = 0; t < NUM_THREADS; t++){
        unsigned int start_idx = t * KEYS_PER_THREAD;
        unsigned int end_idx = (t == NUM_THREADS - 1) ? SINGLE_DIMENSION_KEY_LIMIT : (t + 1) * KEYS_PER_THREAD;
        threads.emplace_back(count_worker, t, start_idx, end_idx);
      }
      
      // Wait for all threads
      for(auto &thread : threads){
        thread.join();
      }
      
      // Aggregate thread-local results into DataDist
      for(int t = 0; t < NUM_THREADS; t++){
        for(auto gc = 0; gc < nGridCells; gc++){
          DataDist[gc] += thread_local_dists[t][gc];
        }
      }
    }
    else{
      for(unsigned int i = 0; i < this->idx->objects_.size(); i++){
          double lx = this->idx->objects_[i]->left_;
          double hx = this->idx->objects_[i]->right_;
          double ly = this->idx->objects_[i]->bottom_;
          double hy = this->idx->objects_[i]->top_;

          for (auto gc = 0; gc < nGridCells; gc++){
              double glx = glbGridCell[gc].lx;
              double gly = glbGridCell[gc].ly;
              double ghx = glbGridCell[gc].hx;
              double ghy = glbGridCell[gc].hy;

              if (hx < glx || lx > ghx || hy < gly || ly > ghy)
                  continue;
              else {
                  DataDist[gc]++;
              }        
          }
      }
    }
}

void GridManager::printDataDistIdx(){
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------DataDistribution-------------------------------------" << endl;
    cout << "" << endl;

    for(auto j = 0; j < this->yPar; j++){
        for (auto i = 0; i < this->xPar; i++){
            cout << "|\t" <<  
                DataDist[this->yPar*i + j] <<
                "\t|";
        }
        cout << endl;
    }
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------------------------------------------------------------" << endl;
}
void GridManager::printDataDistIdxT(){
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------DataDistribution-------------------------------------" << endl;
    cout << "" << endl;

    for (auto i = 0; i < this->xPar; i++){
        for(auto j = 0; j < this->yPar; j++){
            cout << "|\t" <<  
                DataDist[this->xPar*j + i] <<
                "\t|";
        }
        cout << endl;
    }
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------------------------------------------------------------" << endl;
}

void GridManager::printQueryView(){
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "------------------------------------Query View-----------------------------------" << endl;
    cout << "QueryDistribution" << endl;
    for(auto j = 0; j < this->yPar; j++){
        for (auto i = 0; i < this->xPar; i++){
            cout << "|\t" << "(" << 
                glbGridCell[this->yPar*i + j].qType[0] <<
                ", " << 
                glbGridCell[this->yPar*i + j].qType[1] <<
                ", " << 
                glbGridCell[this->yPar*i + j].qType[2] <<
            ")" << "\t|";
        }
        cout << endl;
    }
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------------------------------------------------------------" << endl;
}

void GridManager::printQueryCorrMatrixView(){
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "------------------------------------CorrMatrix View-----------------------------------" << endl;
    cout << "CorrMatrix View" << endl;
    for(auto i = 0; i < 10; i++){
        for (auto j = 0; j < 10; j++){
            cout << " " << 
                qCorrMatrix[i][j]
            << " ";
        }
        cout << endl;
    }
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------------------------------------------------------------" << endl;
}

// -------------------------------------------------------------------------------------
// Dynamic Reconfiguration Methods
// -------------------------------------------------------------------------------------

void GridManager::reload_configuration(string configFile) {
    cout << "Reloading configuration from: " << configFile << endl;

    ifstream ifs(configFile, std::ifstream::in);
    if (!ifs.is_open()) {
        cerr << "ERROR: Failed to open config file: " << configFile << endl;
        return;
    }

    vector<NUMAID> numaConfig;
    vector<CPUID> cpuConfig;

    // Read new NUMA assignments
    for (int i = 0; i < nGridCells; i++) {
        NUMAID nID;
        ifs >> nID;
        numaConfig.push_back(nID);
    }

    // Read new CPU assignments
    for (int i = 0; i < nGridCells; i++) {
        CPUID cpuID;
        ifs >> cpuID;
        cpuConfig.push_back(cpuID);
    }

    ifs.close();

    // CRITICAL SECTION: Acquire exclusive write lock to update configuration
    // This blocks all router threads from reading idCPU/idNUMA during update
    // to ensure they never route queries to suboptimal cores
    {
        std::unique_lock<std::shared_mutex> lock(config_mutex);

        // Update grid cells with new assignments (in-place, no reallocation)
        for (int i = 0; i < nGridCells; i++) {
            this->glbGridCell[i].prev_idNUMA = this->glbGridCell[i].idNUMA;
            this->glbGridCell[i].prev_idCPU = this->glbGridCell[i].idCPU;
            this->glbGridCell[i].idNUMA = numaConfig[i];
            this->glbGridCell[i].idCPU = cpuConfig[i];
            this->glbGridCell[i].has_migrated = false;
        }
    }  // Lock released here

    cout << "Configuration reloaded successfully. Grid cells updated." << endl;
}

} // namespace dm
}  // namespace erebus