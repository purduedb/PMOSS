#pragma once
#include <iostream>
#include <fstream>
// -------------------------------------------------------------------------------------
#include "utils/Misc.hpp"
#include "storage/rtree/rtree.h"
// -------------------------------------------------------------------------------------
using std::ifstream;
using std::ofstream;
// -------------------------------------------------------------------------------------
#define MAX_GRID_CELL 1000
#define STAMP_LR_PARAM 4  // For now think of the query MBR as only output
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
      // -------------------------------------------------------------------------------------
      // Model Parameters for stamping query: Currently we have linear regression
      double lRegCoeff[2][STAMP_LR_PARAM];
      // -------------------------------------------------------------------------------------
      // Defines the number of queries of different view, usef for classifying if a grid is MICE, ...
      double qType[3] = {0};
    };
    
    GridCell glbGridCell[MAX_GRID_CELL];
    
    int freqQueryDistPushed[MAX_GRID_CELL] = {0};  // TODO: this needs to be thread-safe
    int freqQueryDistCompleted[MAX_GRID_CELL] = {0};  // TODO: this needs to be thread-safe

    int DataDist[MAX_GRID_CELL] = {0};
    GridManager(int xPar, int yPar, double minXSpace, double maxXSpace, double minYSpace, double maxYSpace);
    void register_grid_cells();
    void register_grid_cells(vector<CPUID> availCPUs);
    void register_index(erebus::storage::rtree::RTree *idx);
    void printGM();
    void printQueryDistPushed();
    void printQueryDistCompleted();
    void printQueryDistOstanding();
    
    void buildDataDistIdx();
    void printDataDistIdx();
    
    void printQueryView();
    

};



}  // namespace dm
}  // namespace erebus