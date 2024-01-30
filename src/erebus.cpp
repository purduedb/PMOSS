#include "erebus.hpp"
// -------------------------------------------------------------------------------------
#include <iostream>
#include <fstream>
#include <thread>   // std::thread
#include <mutex>
// -------------------------------------------------------------------------------------
using std::ifstream;
using std::ofstream;
// -------------------------------------------------------------------------------------
namespace erebus
{
Erebus::Erebus(erebus::storage::rtree::RTree *idx, erebus::dm::GridManager *gm, erebus::scheduler::ResourceManager *rm, erebus::tp::TPManager *tp)
{
	this->idx = idx;
	this->glb_gm = gm;
	this->glb_rm = rm;
	this->glb_tpool = tp;
}

Erebus::Erebus(erebus::storage::rtree::RTree *idx, erebus::dm::GridManager *gm, erebus::scheduler::ResourceManager *rm)
{
	this->idx = idx;
	this->glb_gm = gm;
	this->glb_rm = rm;
}

Erebus::Erebus(erebus::dm::GridManager *gm, erebus::scheduler::ResourceManager *rm)
{
	
	this->glb_gm = gm;
	this->glb_rm = rm;
}

erebus::storage::rtree::RTree* Erebus::build_idx(int insert_strategy, int split_strategy) 
{
	this->idx = ConstructTree(50, 20);
	SetDefaultInsertStrategy(this->idx, insert_strategy);
	SetDefaultSplitStrategy(this->idx, split_strategy);
	int total_access = 0;
	ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in);
	for (int i = 0; i < 5000000; i++) {
		double l, r, b, t;
		ifs >> l >> r >> b >> t;
		Rectangle* rectangle = InsertRec(this->idx, l, r, b, t);
		DefaultInsert(this->idx, rectangle);
	}
	ifs.close();
	cout << this->idx->height_ << " " << GetIndexSizeInMB(this->idx) << endl;
	return this->idx;
}

void Erebus::register_threadpool(erebus::tp::TPManager *tp)
{
	this->glb_tpool = tp;
}

}   // namespace erebus


int main()
{	
	std::vector<CPUID> mm_cpuids;
	std::vector<CPUID> wrk_cpuids;
	std::vector<CPUID> rt_cpuids;
	
	int nNUMANodes = numa_num_configured_nodes();
	int nCPUCores = numa_num_possible_cpus();
	
	vector<CPUID> cPool[nNUMANodes];

	for(auto n=0; n < nNUMANodes; n++){
		struct bitmask *bmp = numa_allocate_cpumask();
		numa_node_to_cpus(n, bmp);
		for(auto j = 0; j < nCPUCores; j++){
			if (numa_bitmask_isbitset(bmp, j)) cPool[n].push_back(j);
		}
	}
	/**
	 * TODO: cleaner version would check the number of rt/numa node and allocate accordingly 
	 * Also would remove the number from the TPM.cc file about the #of threads and make it 
	 * global
	*/
	for(auto n=0; n < nNUMANodes; n++){
		mm_cpuids.push_back(cPool[n][0]);
		rt_cpuids.push_back(cPool[n][1]);
		int cnt = 1;
		for(auto j = 2; j < cPool[n].size(); j++, cnt++){
			wrk_cpuids.push_back(cPool[n][j]);
			if (cnt == 5) break;
		}
	}
	

	// std::vector<CPUID> mm_cpuids = {101, 102};
	// std::vector<CPUID> wrk_cpuids;
	// std::vector<CPUID> rt_cpuids = {0, 12, 24, 36, 48, 60, };
	// Allocate 6 threads off each NUMA Node as worker CUPIDS

	double min_x = -83.478714;
    double max_x = -65.87531;
    double min_y = 38.78981;
    double max_y = 47.491634;
	erebus::dm::GridManager glb_gm(10, 10, min_x, max_x, min_y, max_y);
	
	erebus::scheduler::ResourceManager glb_rm;
	
	erebus::Erebus db(&glb_gm, &glb_rm);
	db.build_idx(1, 1);
	
	glb_gm.register_index(db.idx);
	glb_gm.register_grid_cells(wrk_cpuids);
	glb_gm.printGM();
	glb_gm.printQueryDist();
	glb_gm.buildDataDistIdx();
	glb_gm.printDataDistIdx();
	
	
	erebus::tp::TPManager glb_tpool(mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);
	glb_tpool.initWorkerThreads();
	glb_tpool.initRouterThreads();
	glb_tpool.initMegaMindThreads();

	std::this_thread::sleep_for(std::chrono::milliseconds(50000));
	glb_tpool.terminateWorkerThreads();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	glb_gm.printQueryDist();
	glb_tpool.dumpGridHWCounters(-1);
	// while(1){
	// 	glb_gm.printQueryDist();
	// 	std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	// }
	
	while(1);
	return 0;

	// BS::thread_pool MegaMindPool(4);
	// BS::thread_pool WorkerPool(50);
	// auto mtids = MegaMindPool.get_thread_ids();
	// auto wtids = WorkerPool.get_thread_ids();

	// for (std::thread::id i: mtids) cout << i << ' '; cout << endl;
	// for (std::thread::id i: wtids) cout << i << ' '; cout << endl;

	// return 0;

	// tf::Executor executor;
  	// tf::Taskflow taskflow;

	// auto [A, B, C, D] = taskflow.emplace(  // create four tasks
	// 	[] () { std::cout << "TaskA\n"; },
	// 	[] () { std::cout << "TaskB\n"; },
	// 	[] () { std::cout << "TaskC\n"; },
	// 	[] () { std::cout << "TaskD\n"; } 
	// );                                  
										
	// A.precede(B, C);  // A runs before B and C
	// D.succeed(B, C);  // D runs after  B and C
										
	// executor.run(taskflow).wait(); 

	// return 0;

    // build_basertree(1, 1);

    // unsigned num_cpus = std::thread::hardware_concurrency();
    // std::cout << "Launching " << num_cpus << " threads\n";
    
    // constexpr unsigned num_threads = 20;
    // std::vector<std::thread> threads(num_threads);
    // std::mutex iomutex;
    

	// test_loop_until_empty();
    // test_init_thread();

   
}