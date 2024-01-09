
#pragma once
// -------------------------------------------------------------------------------------
#include <unordered_map>
#include <thread>
// -------------------------------------------------------------------------------------
#include "shared-headers/Units.hpp"

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
        std::unordered_map<u64, std::thread*> NUMAToThread;
        // -------------------------------------------------------------------------------------
        ResourceManager();
        ~ResourceManager();
        // -------------------------------------------------------------------------------------
        void register_cpu(int cpuid, std::thread* th);
        void register_numa(int numaid, std::thread* th);
};

}  // namespace scheduler
}  // namespace erebus