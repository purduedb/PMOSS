#pragma once

#include <numa.h>
// -------------------------------------------------------------------------------------

#include "threads/TPM.hpp"
#include "scheduling/RM.hpp"
#include "scheduling/GM.hpp"

// -------------------------------------------------------------------------------------

namespace erebus
{
class Erebus
{
  public:
    erebus::storage::rtree::RTree *idx;
    erebus::storage::qtree::QuadTree *idx_qtree;
    erebus::storage::BTreeOLCIndex<keytype, keycomp> *idx_btree;

    erebus::dm::GridManager *glb_gm;
    erebus::scheduler::ResourceManager *glb_rm;
    erebus::tp::TPManager *glb_tpool;
    
    // -------------------------------------------------------------------------------------
    Erebus(erebus::dm::GridManager *gm, erebus::scheduler::ResourceManager *rm);
    // ~Erebus();
    
    // -------------------------------------------------------------------------------------
    erebus::storage::rtree::RTree* build_rtree(int ds, int insert_strategy, int split_strategy);
    erebus::storage::qtree::QuadTree* build_idx(float min_x, float max_x, float min_y, float max_y);
    erebus::storage::BTreeOLCIndex<keytype, keycomp>* build_btree(const uint64_t wl, const uint64_t kt, std::vector<keytype> &init_keys, 
      std::vector<uint64_t> &values);
    void register_threadpool(erebus::tp::TPManager *tp);
};

}  // namespace erebus