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
    void register_threadpool(erebus::tp::TPManager *tp);
};

}  // namespace erebus