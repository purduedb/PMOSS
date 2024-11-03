# P-MOSS: Learned Scheduling For Indexes Over NUMA Servers Using Low-Level Hardware Statistic
P-MOSS is a learned Performance MOnitoring Unit (PMU) driven Spatial Query Scheduling framework, that utilizes spatial query scheduling to improve the query execution performance of main memory indexes in NUMA servers. There are 2 component to PMOSS: a system component and a learned component.
--------------------------------------------------------------------------------
Building System Componet of PMOSS
--------------------------------------------------------------------------------
Install cmake (and libasan on Linux) then:
```
mkdir build
cd build
cmake ..
cmake --build .
```