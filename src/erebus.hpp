#pragma once

// -------------------------------------------------------------------------------------
#include "utils/gflags.h"
#include "threads/TPM.hpp"
#include "scheduling/RM.hpp"
#include "scheduling/GM.hpp"
#include <numa.h>
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
    // ~Erebus();
    // -------------------------------------------------------------------------------------
    void register_idx(int insert_strategy, int split_strategy);
    void register_threadpool(erebus::tp::TPManager *tp);
};

}  // namespace erebus