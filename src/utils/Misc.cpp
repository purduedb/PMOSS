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

int CntHWThreads()
{
   unsigned num_cpus = std::thread::hardware_concurrency();
   return num_cpus;
}

void printGrid(int userRows, int userColumns){
    /**
     * https://stackoverflow.com/questions/48677066/printing-a-grid
    */

    cout << endl;
    cout << " ";
    int i = 1, j;
    for(j = 0; j <= 4 * userColumns; j++){
        if (j%4==2 )
            cout << i++;
        else cout<<" ";
    }
    cout<<endl;
    
    for(i = 0; i <= 2*userRows; i++){
        if(i%2!=0){
            cout<<(char)(i/2 +'A');
        }
        for(j = 0; j <= 2*userColumns; j++){
            if(i%2==0)
            {
                if(j==0)
                    cout<<" ";
                if(j%2==0)
                    cout<<" ";
                else cout<<"---";
            }
            else{
                if(j%2==0)
                    cout<<"|";
                else cout<<"   ";
            }
        }
        if(i%2!=0)
            cout<<(char)(i/2 +'A');
        cout<<endl;
    }
    cout<<" ";
    for(j = 0, i = 1; j <= 4*userColumns; j++){
        if(j%4==2)
            cout<<i++;
        else cout<<" ";
    }
    cout<<endl;
}

}  // namespace utils
}  // namespace erebus