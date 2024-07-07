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
    erebus::storage::qtree::QuadTree *idx_qtree;  // everywhere where is this->idx needs to be replaced by this
    
    erebus::dm::GridManager *glb_gm;
    erebus::scheduler::ResourceManager *glb_rm;
    erebus::tp::TPManager *glb_tpool;
    // -------------------------------------------------------------------------------------
    Erebus(erebus::storage::rtree::RTree *idx, erebus::dm::GridManager *gm, erebus::scheduler::ResourceManager *rm, erebus::tp::TPManager *tp);
    Erebus(erebus::storage::rtree::RTree *idx, erebus::dm::GridManager *gm, erebus::scheduler::ResourceManager *rm);
    Erebus(erebus::dm::GridManager *gm, erebus::scheduler::ResourceManager *rm);
    // ~Erebus();
    // -------------------------------------------------------------------------------------
    erebus::storage::rtree::RTree* build_idx(int insert_strategy, int split_strategy);
    erebus::storage::qtree::QuadTree* build_idx(float min_x, float max_x, float min_y, float max_y);
    void register_threadpool(erebus::tp::TPManager *tp);
};

}  // namespace erebus