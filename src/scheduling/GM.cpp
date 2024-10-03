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
  ifs.close();

	std::vector<double> xList = utils::linspace<double>(this->minXSpace, this->maxXSpace, this->xPar+1);
	std::vector<double> yList = utils::linspace<double>(this->minYSpace, this->maxYSpace, this->yPar+1);
	double delX = xList[1] - xList[0];
	double delY = yList[1] - yList[0];
	
	// -------------------------------------------------------------------------------------
	int trk_cid = 0;
  
	// auto start = std::chrono::high_resolution_clock::now();

	for(auto i = 0; i < this->xPar; i++){
		for (auto j = 0; j < this->yPar; j++){

			this->glbGridCell[trk_cid].cid = trk_cid;
			
			this->glbGridCell[trk_cid].lx = xList[i];
			this->glbGridCell[trk_cid].ly = yList[j];
			this->glbGridCell[trk_cid].hx = xList[i]+delX;
			this->glbGridCell[trk_cid].hy = yList[j]+delY;
			
			
			this->glbGridCell[trk_cid].idNUMA = numaConfig[trk_cid];
			this->glbGridCell[trk_cid].idCPU = cpuConfig[trk_cid]; 
			
		// -------------------------------------------------------------------------------------
      trk_cid++;
			
		}
	}
}


void GridManager::enforce_scheduling(){
  auto start = std::chrono::high_resolution_clock::now();
  for(size_t i = 0; i < MAX_GRID_CELL; i++){
    double lx = this->glbGridCell[i].lx;
    double hx = this->glbGridCell[i].hx;
    double ly = this->glbGridCell[i].ly;
    double hy = this->glbGridCell[i].hy;
    int numa_id = this->glbGridCell[i].idNUMA;
  #if LINUX != 0
		#if STORAGE == 0
			MigrateNodes(this->idx, lx, hx, ly, hy, numa_id);    
		#elif STORAGE == 1
			MigrateNodesQuad(this->idx_quadtree, lx, hx, ly, hy, numa_id);    
		#elif STORAGE == 2
			this->idx_btree->migrate_(lx, this->DataDist[i], numa_id);
		#endif
	#endif
  }
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
    if (access_method == BTREE){
      for(unsigned int i = 0; i < BTREE_INIT_LIMIT; i++){
          double lx = init_keys[i];

          for (auto gc = 0; gc < nGridCells; gc++){
            double glx = glbGridCell[gc].lx;
            double ghx = glbGridCell[gc].hx;
      
            if (lx <= ghx && lx >= glx)
              DataDist[gc]++;
            else 
              continue;       
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


} // namespace dm
}  // namespace erebus