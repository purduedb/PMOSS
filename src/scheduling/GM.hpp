#pragma once
#include "utils/Misc.hpp"
#include "storage/rtree/rtree.h"
// -------------------------------------------------------------------------------------
#define MAX_GRID_CELL 1000
// -------------------------------------------------------------------------------------

namespace erebus
{
namespace dm
{
class GridManager
{
  public:
    int xPar;  // #lines in x-dim
    int yPar;  // #lines in y-dim
    double minXSpace;  // min-space of the index in x-dim
    double minYSpace;  // min-space of the index in y-dim
    double maxXSpace;  // min-space of the index in x-dim
    double maxYSpace;  // min-space of the index in y-dim
    int nGridCells =  (this->xPar) *  (this->yPar);
    // -------------------------------------------------------------------------------------
    erebus::storage::rtree::RTree *idx;
    // -------------------------------------------------------------------------------------

    struct GridCell{
      int cid;
      // -------------------------------------------------------------------------------------
      double lx;
      double ly;
      double hx;
      double hy;
      // -------------------------------------------------------------------------------------
      int idNUMA;
      int idCPU;
    };
    
    GridCell glbGridCell[MAX_GRID_CELL];

    GridManager(int xPar, int yPar, double minXSpace, double maxXSpace, double minYSpace, double maxYSpace);
    void register_grid_cells();

};



}  // namespace dm
}  // namespace erebus