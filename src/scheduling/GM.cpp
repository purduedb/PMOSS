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
        this->nGridCells = (this->xPar+1)*(this->yPar+1);
        this->idx = nullptr;
        
    }
    
    void GridManager::register_grid_cells(){
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
                
                trk_cid++; 
            }
        }
    }
}  // namespace dm
}  // namespace erebus