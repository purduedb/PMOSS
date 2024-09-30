#ifndef PMOSS_GRIDMANAGER_H_
#define PMOSS_GRIDMANAGER_H_

#include <iostream>
#include <fstream>
// -------------------------------------------------------------------------------------
#include "utils/Misc.hpp"
#include "storage/index.h"  
#include "storage/btree/btree.h"  
#include "storage/rtree/rtree.h"
#include "storage/qtree/qtree.h"  
// -------------------------------------------------------------------------------------
using std::ifstream;
using std::ofstream;
// -------------------------------------------------------------------------------------
#define SINGLE_DIMENSION_KEY_LIMIT 100000000       
#define BTREE_INIT_LIMIT 30000000
#define LIMIT 1000                  // test btree workload
#define MAX_GRID_CELL 100
#define STAMP_LR_PARAM 4            // For now think of the query MBR as only output
#define MAX_XPAR 10
#define MAX_YPAR 10
// -------------------------------------------------------------------------------------
# define USE_MODEL 0 
// -------------------------------------------------------------------------------------
#define MULTIDIM 0 
#define STORAGE 2  // RTree(0), QTree(1), BTree
#define DATASET 3  // OSM(0), GEOLIFE(1), BMOD02(2), ycsb (3)
#define MACHINE 3 // 0 (BIGDATA), 1(DBSERVER), 2 (AMD)
#define LINUX 3 // 0 (SE 0, SE-NUMA 1, SN-NUMA 2, OURS, 3)
// -------------------------------------------------------------------------------------
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
    int wkload;         // workload
    int iam;
    int xPar;           // #lines in x-dim
    int yPar;           // #lines in y-dim
    double minXSpace;   // min-space of the index in x-dim
    double minYSpace;   // min-space of the index in y-dim
    double maxXSpace;   // min-space of the index in x-dim
    double maxYSpace;   // min-space of the index in y-dim
    int nGridCells;
    int nGridCellsPerThread;
    std::atomic <int> btree_key_count;
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
    
    GridManager(int config, int wkload, int iam, int xPar, int yPar, double minXSpace, double maxXSpace, double minYSpace, double maxYSpace);
    void register_grid_cells();
    void register_grid_cells(vector<CPUID> availCPUs);
    void register_grid_cells(string configFile);
    void register_grid_cells_parallel(string configFile);
    void register_index(erebus::storage::rtree::RTree *idx);
    void register_index(erebus::storage::qtree::QuadTree *idx_quadtree);
    void register_index(erebus::storage::BTreeOLCIndex<keytype, keycomp> *idx_btree);
    void enforce_scheduling();
    
    void printGM();
    void printQueryDistPushed();
    void printQueryDistCompleted();
    void printQueryDistOstanding();
    
    void buildDataDistIdx(int access_method, std::vector<keytype> &init_keys);
    void printDataDistIdx();
    void printDataDistIdxT();
    
    void printQueryView();
    void printQueryCorrMatrixView();
    
    // void GMMigrate();

};



}  // namespace dm
}  // namespace erebus


#endif // PMOSS_GRIDMANAGER_H_