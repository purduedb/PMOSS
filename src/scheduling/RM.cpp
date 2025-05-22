#include "RM.hpp"
// -------------------------------------------------------------------------------------
namespace erebus
{
namespace scheduler
{
ResourceManager::ResourceManager()
{
    // // -------------------------------------------------------------------------------------
    // for(auto i=0; i < TPM->CURR_ROUTER_THREADS; i++)
    // {
    //     int cpuid = TPM->router_threads_meta[i].cpuid;
    //     std::thread * th = &(TPM->glb_router_thrds[i]);
    //     CPUCoreToThread[cpuid]=th;
    // }
    // for(auto i=0; i < TPM->CURR_WORKER_THREADS; i++)
    // {
    //     int cpuid = TPM->worker_threads_meta[i].cpuid;
    //     std::thread * th = &(TPM->glb_worker_thrds[i]);
    //     CPUCoreToThread[cpuid]=th;
    // }
    // for(auto i=0; i < TPM->CURR_MEGAMIND_THREADS; i++)
    // {
    //     int cpuid = TPM->megamind_threads_meta[i].cpuid;
    //     std::thread * th = &(TPM->glb_megamind_thrds[i]);
    //     CPUCoreToThread[cpuid]=th;
        
    // }
    // -------------------------------------------------------------------------------------
}

ResourceManager::~ResourceManager()
{
    this->CPUCoreToThread.clear();
}

void ResourceManager::register_cpu(int cpuid, std::thread *th){
    CPUCoreToThread[cpuid]=th;
}

void ResourceManager::register_numa(int numaid, std::thread *th){
    NUMAToThread[numaid]=th;
}
}  // nampespace scheduler
}  // nampespace erebus