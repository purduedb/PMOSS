#pragma once
#include <iostream>
#include <fstream>
// -------------------------------------------------------------------------------------
#include "utils/Misc.hpp"
#include "storage/index.h"  
#include "storage/rtree/rtree.h"
#include "storage/qtree/qtree.h"  
// -------------------------------------------------------------------------------------
using std::ifstream;
using std::ofstream;
// -------------------------------------------------------------------------------------
#define INIT_LIMIT 5000000   // this is for btree index building
#define LIMIT 1000  // this is for btree workload
#define MAX_GRID_CELL 100
#define STAMP_LR_PARAM 4  // For now think of the query MBR as only output
#define MAX_XPAR 10
#define MAX_YPAR 10
// -------------------------------------------------------------------------------------
# define USE_MODEL 0 
// -------------------------------------------------------------------------------------
#define MULTIDIM 0 
#define STORAGE 2  // RTree(0), QTree(1), BTree
#define DATASET 3  // OSM(0), GEOLIFE(1), BMOD02(2), ycsb (3)
#define MACHINE 0 // 0 (BIGDATA), 1(DBSERVER)
#define LINUX 3 // 0 (SE 0, SE-NUMA 1, SN, NUMA 2, OURS, 3)
// -------------------------------------------------------------------------------------
#define WKLOAD 30
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

namespace erebus
{
namespace dm
{
class GridManager
{
  public:
    int config;         // hw config of the grid manager
    int xPar;           // #lines in x-dim
    int yPar;           // #lines in y-dim
    double minXSpace;   // min-space of the index in x-dim
    double minYSpace;   // min-space of the index in y-dim
    double maxXSpace;   // min-space of the index in x-dim
    double maxYSpace;   // min-space of the index in y-dim
    int nGridCells;
    int nGridCellsPerThread;
    // -------------------------------------------------------------------------------------
    erebus::storage::rtree::RTree *idx;
    erebus::storage::qtree::QuadTree *idx_quadtree;
    erebus::storage::BTreeOLCIndex<keytype, keycomp> *idx_btree;
    // -------------------------------------------------------------------------------------
    std::unordered_map<u64, std::thread*> CPUCoreToThread;
    std::unordered_multimap<u64, CPUID> NUMAToWorkerCPUs;
    std::unordered_multimap<u64, CPUID> NUMAToRoutingCPUs;
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
    // -------------------------------------------------------------------------------------
    // Correlation Query Matrix of the grid cells [NUM_GRID_CELLS x NUM_GRID_CELLS]
    // Update: Now each router thread has this
    int qCorrMatrix[MAX_GRID_CELL][MAX_GRID_CELL] = {0};  //It needs to be thread-safe
    // -------------------------------------------------------------------------------------
    
    int freqQueryDistPushed[MAX_GRID_CELL] = {0};  // TODO: this needs to be thread-safe
    int freqQueryDistCompleted[MAX_GRID_CELL] = {0};  // TODO: this needs to be thread-safe

    // int DataDist[MAX_GRID_CELL] = {0};
    vector<int> DataDist;
    
    GridManager(int config, int xPar, int yPar, double minXSpace, double maxXSpace, double minYSpace, double maxYSpace);
    void register_grid_cells();
    void register_grid_cells(vector<CPUID> availCPUs);
    void register_grid_cells(string configFile);
    void register_index(erebus::storage::rtree::RTree *idx);
    void register_index(erebus::storage::qtree::QuadTree *idx_quadtree);
    void register_index(erebus::storage::BTreeOLCIndex<keytype, keycomp> *idx_btree);
    
    void printGM();
    void printQueryDistPushed();
    void printQueryDistCompleted();
    void printQueryDistOstanding();
    
    void buildDataDistIdx();
    void printDataDistIdx();
    void printDataDistIdxT();
    
    void printQueryView();
    void printQueryCorrMatrixView();
    
    // void GMMigrate();

};



}  // namespace dm
}  // namespace erebus