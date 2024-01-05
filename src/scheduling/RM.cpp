#include "RM.hpp"
// -------------------------------------------------------------------------------------
namespace erebus
{
namespace scheduler
{
ResourceManager::ResourceManager(tp::TPManager *TPM, storage::rtree::RTree* TreeIdx)
{
    this->TreeIdx = TreeIdx;
    this->TPM = TPM;
    
    for(auto i=0; i < TPM->CURR_ROUTER_THREADS; i++)
    {
        int cpuid = TPM->router_threads_meta[i].cpuid;
        std::thread * th = &(TPM->glb_router_thrds[i]);
        CPUCoreToThread[cpuid]=th;
    }
    for(auto i=0; i < TPM->CURR_WORKER_THREADS; i++)
    {
        int cpuid = TPM->worker_threads_meta[i].cpuid;
        std::thread * th = &(TPM->glb_worker_thrds[i]);
        CPUCoreToThread[cpuid]=th;
    }
    // for(auto i=0; i < TPM->CURR_MEGAMIND_THREADS; i++)
    // {
    //     int cpuid = TPM->megamind_threads_meta[i].cpuid;
    //     std::thread * th = &(TPM->glb_megamind_thrds[i]);
        
    // }

    for (const auto & [ key, value ] : CPUCoreToThread) {
        cout << key << ": " << value->get_id() << endl;
    }
    
}

ResourceManager::~ResourceManager()
{
    this->TPM = nullptr;
    this->TreeIdx = nullptr;
    this->CPUCoreToThread.clear();
}
}  // nampespace scheduler
}  // nampespace erebus