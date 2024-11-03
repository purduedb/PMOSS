#include "RM.hpp"
// -------------------------------------------------------------------------------------
namespace erebus
{
namespace scheduler
{
ResourceManager::ResourceManager()
{
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