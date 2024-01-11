#pragma once
// -------------------------------------------------------------------------------------
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
    int nGridCells;
    int nGridCellsPerThread;
    // -------------------------------------------------------------------------------------
    erebus::storage::rtree::RTree *idx;
    // -------------------------------------------------------------------------------------
    std::unordered_map<u64, std::thread*> CPUCoreToThread;
    std::unordered_map<u64, std::thread*> NUMAToThread;
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
    void register_grid_cells(vector<CPUID> availCPUs);
    void register_index(erebus::storage::rtree::RTree *idx);

};



}  // namespace dm
}  // namespace erebus