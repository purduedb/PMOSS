#pragma once
#include <thread>
#include <vector>
#include <numa.h>
// -------------------------------------------------------------------------------------
#include "shared-headers/Units.hpp"
// -------------------------------------------------------------------------------------
namespace erebus{
namespace utils{

void PinThisThread(const u64 t_i);
int CntHWThreads();
/**
 * Copied from https://gist.github.com/lorenzoriano/5414671
*/
template <typename T>
std::vector<T> linspace(double start, double end, double num)
{
    std::vector<T> linspaced;

    if (0 != num)
    {
        if (1 == num) 
        {
            linspaced.push_back(static_cast<T>(start));
        }
        else
        {
            double delta = (end - start) / (num - 1);

            for (auto i = 0; i < (num - 1); ++i)
            {
                linspaced.push_back(static_cast<T>(start + delta * i));
            }
            // ensure that start and end are exactly the same as the input
            linspaced.push_back(static_cast<T>(end));
        }
    }
    return linspaced;
}

void printGrid(int userRows, int userColumns);


}  // namespace utils
}  // namespace erebus