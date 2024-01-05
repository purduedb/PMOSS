
// -------------------------------------------------------------------------------------
#include "threads/threadpool.hpp"
// -------------------------------------------------------------------------------------

namespace erebus
{
namespace scheduler
{
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
/*
    Ties Hardware to Threads
    Need another that ties the grid to Hardware
 */
class ResourceManager
{
    public:
        std::unordered_map<u64, std::thread*> CPUCoreToThread;
        storage::rtree::RTree* TreeIdx;
        tp::TPManager *TPM;
        // -------------------------------------------------------------------------------------
        ResourceManager(tp::TPManager *TPM, storage::rtree::RTree* TreeIdx);
        ~ResourceManager();
        // -------------------------------------------------------------------------------------
};

}  // namespace scheduler
}  // namespace erebus