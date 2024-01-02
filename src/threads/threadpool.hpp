/**
 * @file threadpool.hpp
 * @author yr (Dec17,23)
 * @brief 
 * 
 */

/**
 * TODO: (a) create n(=num_cores) threads, bind it to cores,
 * (b) Have something to point to the active task sets that a thread needs to execute
 * [Probably a grid data structure where all the queries that fall in a cell are queued]
 * 
 */

#pragma once
#include "shared-headers/Units.hpp"
#include <thread>
#include <vector>
#include <mutex>

namespace erebus
{
namespace tp
{
    
class ThreadPool{
  public:
    static ThreadPool* glb_tpool;
    std::vector<std::thread> glb_thrds; 
    
    ThreadPool();

};


}

}
