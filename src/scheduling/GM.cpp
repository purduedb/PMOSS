#include "GM.hpp"
// -------------------------------------------------------------------------------------


namespace erebus
{
namespace dm
{
GridManager::GridManager(int xPar, int yPar, double minXSpace, double maxXSpace, double minYSpace, double maxYSpace){
    this->xPar = xPar;
    this->yPar = yPar;
    this->minXSpace = minXSpace;
    this->maxXSpace = maxXSpace;
    this->minYSpace = minYSpace;
    this->maxYSpace = maxYSpace;
    
    this->nGridCells = this->xPar * this->yPar;
    this->idx = nullptr;
}

void GridManager::register_grid_cells(){
    int nGridCellsPerThread = this->nGridCells / 40 + 1; // utils::CntHWThreads();
    std::vector<double> xList = utils::linspace<double>(this->minXSpace, this->maxXSpace, this->xPar+1);
    std::vector<double> yList = utils::linspace<double>(this->minYSpace, this->maxYSpace, this->yPar+1);
    double delX = xList[1] - xList[0];
    double delY = yList[1] - yList[0];
    int trk_cid = 0;
    for(auto i = 0; i < this->xPar; i++){
        for (auto j = 0; j < this->yPar; j++){
            this->glbGridCell[trk_cid].cid = trk_cid;
            
            this->glbGridCell[trk_cid].lx = xList[i];
            this->glbGridCell[trk_cid].ly = yList[j];
            this->glbGridCell[trk_cid].hx = xList[i]+delX;
            this->glbGridCell[trk_cid].hy = yList[j]+delY;
            this->glbGridCell[trk_cid].idCPU = trk_cid/nGridCellsPerThread;
            
            trk_cid++; 
        }
    }
}
void GridManager::register_grid_cells(vector<CPUID> availCPUs){
    int nGridCellsPerThread = this->nGridCells / availCPUs.size() + 1; 

    std::vector<double> xList = utils::linspace<double>(this->minXSpace, this->maxXSpace, this->xPar+1);
    std::vector<double> yList = utils::linspace<double>(this->minYSpace, this->maxYSpace, this->yPar+1);
    double delX = xList[1] - xList[0];
    double delY = yList[1] - yList[0];
    
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
            
            trk_cid++; 
        }
    }
}
void GridManager::register_index(erebus::storage::rtree::RTree * idx)
{
    this->idx = idx;
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

void GridManager::printQueryDist(){
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------QueryDistribution-------------------------------------" << endl;
    cout << "" << endl;

    for(auto j = 0; j < this->yPar; j++){
        for (auto i = 0; i < this->xPar; i++){
            cout << "|\t" <<  
                freqQueryDist[this->yPar*i + j] <<
                "\t|";
        }
        cout << endl;
    }
    cout << "-------------------------------------------------------------------------------------" << endl;
    cout << "-------------------------------------------------------------------------------------" << endl;
}

} // namespace dm
}  // namespace erebus