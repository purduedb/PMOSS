## P-MOSS: Scheduling Main-Memory Indexes Over NUMA Servers Using Next Token Prediction
This repo contains the code for running the OS baselines in the paper, i.e., OS-local, OS-interleave, SE:NUMA, SN:NUMA.

### Prerequisites 
- CMake
- [Intel TBB](https://github.com/oneapi-src/oneTBB) 

### Build instructions
```
mkdir build
cd build
cmake ..
cmake --build .
```
### Running Code
Config for baselines  
- 500: OS local alloc
- 501: OS interleave
- 502: SE: NUMA
- 506: SN: NUMA

```
./run_script.sh
```

### Implementation
```
PMOSS
├── README.md                  # Project README file
├── Kb_b__                     # The ``Offline'' / ``Fine-tuning'' dataset
├── src                        # Root folder for c source code
│   ├── config                 # The scheduling policy for different machines
│   │   ├── amd_epyc7302_2s_2n # The scheduling policy for AMD EPYC 7302 Server with NPS=1
│   │   │   ├── c_500_256.txt  # Policy ID: 500, Number of index slices: 256
│   │   │   ...
│   │   ...
│   ├── profiling              # Folder for profiling logic 
│   │   ├── PCMMem.cpp         # Intel PCM integration and logic for probing the interconnects and memory controllers
│   │   ├── PCMMem.hpp         # Header file for PCMMem.cpp
│   │   ├── PerfCounters.hpp              
│   │   ...
│   ├── scheduling             # Folder for the core logic of scheduling
│   │   ├── GM.cpp             # Logic for dividing the index into slices and enforcing the scheduling policy 
                               # Change the Macro definitions for different machines, or index sizes
                               # BTREE_INIT_LIMIT, MACHINE
│   │   ├── GM.hpp                 
│   │   ├── RM.cpp             
│   │   ├── RM.hpp                
│   ├── shared-headers         # Folder for the specific perf-events we collect
│   │   ├── PerfEvent_amd.hpp  # Perf events for AMD servers
│   │   ├── PerfEvent_arm.hpp  # Perf events for ARM servers
│   │   ...
│   ├── storage                # Folder for index code
│   │   ├── btree              # Folder for B-Tree index operation code
│   │   │   ...
│   │   ├── index.h            # Definition of different index operations
│   │   ├── indexkey.h         # 
│   ├── threads                # Folder for the logic of different threads
│   │   ├── TPM.cc             # Code for different threads: router, worker, core-sweeper, ...
│   │   ├── TPM.hpp            
│   ├── workloads              # Folder for the ycsb workloads
│   │   ├── amd_epyc7302_2s_2n # Workload for AMD EPYC 7302 Server with NPS=1
├── erebus.cpp                 # The entry point of PMOSS project
├── erebus.hpp                  
```