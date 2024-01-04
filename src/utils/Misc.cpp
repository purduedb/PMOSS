#include "Misc.hpp"

namespace erebus{

namespace utils{

void PinThisThread(const u64 t_i)
{
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(t_i, &cpuset);
   pthread_t current_thread = pthread_self();
   if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
      cout << "Could not pin a thread, maybe because of over subscription?" << endl;
   }
}

}
}