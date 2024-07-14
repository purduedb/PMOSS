#include "erebus.hpp"
// -------------------------------------------------------------------------------------
#define TEST_WORKLOAD_INTERFERENCE 0
// -------------------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <thread>   // std::thread
#include <mutex>
// -------------------------------------------------------------------------------------
// #include "../third-party/pcm/src/cpucounters.h"	// Intel PCM monitoring tool
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
	#if MACHINE == 0 
	#if DATASET == 0
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#elif DATASET == 1
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/geo.txt", std::ifstream::in); // 24000000
		int totPoints = 24000000;
	#elif DATASET == 2
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/bmod02.txt", std::ifstream::in);  //11975098
		int totPoints = 11975098;
	#else 
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#endif
	#endif
	#if MACHINE == 1 
	#if DATASET == 0
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#elif DATASET == 1
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/geo.txt", std::ifstream::in); // 24000000
		int totPoints = 24000000;
	#elif DATASET == 2
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/bmod02.txt", std::ifstream::in);  //11975098
		int totPoints = 11975098;
	#else 
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#endif
	#endif
	#if MACHINE == 2 
	#if DATASET == 0
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#elif DATASET == 1
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/geo.txt", std::ifstream::in); // 24000000
		int totPoints = 24000000;
	#elif DATASET == 2
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/bmod02.txt", std::ifstream::in);  //11975098
		int totPoints = 11975098;
	#else 
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#endif
	#endif
	#if MACHINE == 3 
	#if DATASET == 0
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#elif DATASET == 1
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/geo.txt", std::ifstream::in); // 24000000
		int totPoints = 24000000;
	#elif DATASET == 2
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/bmod02.txt", std::ifstream::in);  //11975098
		int totPoints = 11975098;
	#else 
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#endif
	#endif
	
	for (int i = 0; i < totPoints; i++) {
		double l, r, b, t;
		ifs >> l >> r >> b >> t;
		Rectangle* rectangle = InsertRec(this->idx, l, r, b, t);
		DefaultInsert(this->idx, rectangle);
	}
	ifs.close();
	cout << this->idx->height_ << " " << GetIndexSizeInMB(this->idx) << endl;
	
	// int pgCnt = 10;
	// void *pgs[pgCnt];
	// for(auto i = 0; i < pgCnt; i++) pgs[i] = this->idx->tree_nodes_[i];
	// int status [pgCnt];
	// const int destNodes[pgCnt] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1};
	// int ret_code  = move_pages(0, pgCnt, pgs, destNodes, status, 0);

	// for(auto pnode = 0; pnode < pgCnt; pnode++){
	// 	void *ptr_to_check = this->idx->tree_nodes_[pnode];
	// 	int tstatus[1];
	// 	const int tDestNodes[1] = {destNodes[pnode]};
	// 	int tret_code = move_pages(0, 1, &ptr_to_check, tDestNodes, tstatus, 0);
	// 	printf("Memory at %p is at %d node (retcode %d)\n", ptr_to_check, tstatus[0], tret_code);    
		// void *pgs[9] = {
		// 	&(this->idx->tree_nodes_[pnode]->father),
		// 	&(this->idx->tree_nodes_[pnode]->children),
		// 	&(this->idx->tree_nodes_[pnode]->entry_num),
		// 	&(this->idx->tree_nodes_[pnode]->is_overflow),
		// 	&(this->idx->tree_nodes_[pnode]->is_leaf),
		// 	&(this->idx->tree_nodes_[pnode]->origin_center),
		// 	&(this->idx->tree_nodes_[pnode]->maximum_entry),
		// 	&(this->idx->tree_nodes_[pnode]->minimum_entry),
		// 	&(this->idx->tree_nodes_[pnode]->RR_s)
		// 			};
		
		// int pgStatus[9];
		// move_pages(0, 9, pgs, NULL, pgStatus, 0);
		// for(auto i = 0; i < 9; i++)
		// 	cout << pgStatus[i] << " ";
		// cout << endl;
		
		
		// // print all the nodes addresses
		// cout << "\t\t\t-------------------------------------------------------------------------------------" << endl;
		// for(auto cnode = 0; cnode <= pnode; cnode++){
		// 	void *cptr_to_check = this->idx->tree_nodes_[cnode];
		// 	int tcstatus[1];
			
		// 	int ctret_code = move_pages(0, 1, &cptr_to_check, NULL, tcstatus, 0);
		// 	printf("Memory at %p is at %d node (retcode %d)\n", cptr_to_check, tcstatus[0], ctret_code);    

		// }
		// cout << "\t\t\t-------------------------------------------------------------------------------------" << endl;
		
	// }
		
		
	


	return this->idx;
}

erebus::storage::qtree::QuadTree* Erebus::build_idx(float min_x, float max_x, float min_y, float max_y) 
{
	#if MACHINE == 0 
	#if DATASET == 0
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#elif DATASET == 1
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/geo.txt", std::ifstream::in); // 24000000
		int totPoints = 24000000;
	#elif DATASET == 2
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/bmod02.txt", std::ifstream::in);  //11975098
		int totPoints = 11975098;
	#else 
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#endif
	#endif
	#if MACHINE == 1 
	#if DATASET == 0
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#elif DATASET == 1
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/geo.txt", std::ifstream::in); // 24000000
		int totPoints = 24000000;
	#elif DATASET == 2
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/bmod02.txt", std::ifstream::in);  //11975098
		int totPoints = 11975098;
	#else 
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#endif
	#endif
	#if MACHINE == 2 
	#if DATASET == 0
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#elif DATASET == 1
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/geo.txt", std::ifstream::in); // 24000000
		int totPoints = 24000000;
	#elif DATASET == 2
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/bmod02.txt", std::ifstream::in);  //11975098
		int totPoints = 11975098;
	#else 
		ifstream ifs("/home/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#endif
	#endif
	#if MACHINE == 3 
	#if DATASET == 0
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#elif DATASET == 1
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/geo.txt", std::ifstream::in); // 24000000
		int totPoints = 24000000;
	#elif DATASET == 2
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/bmod02.txt", std::ifstream::in);  //11975098
		int totPoints = 11975098;
	#else 
		ifstream ifs("/homes/yrayhan/works/erebus/src/dataset/us.txt", std::ifstream::in); // 100000000
		int totPoints = 90000000;
	#endif
	#endif
	
	this->idx_qtree = new erebus::storage::qtree::QuadTree(
		{min_x, min_y, (max_x-min_x), (max_y-min_y)}, 100, 100
		);
	for (int i = 0; i < totPoints; i++) {
		double l, r, b, t;
		ifs >> l >> r >> b >> t;
		int data = i;
		erebus::storage::qtree::Collidable* obj = new erebus::storage::qtree::Collidable({ l, b, 0, 0 }, data);
		this->idx_qtree->insert(obj);
	}
	std::cout << this->idx_qtree->totalChildren() << "\n";
	std::cout << this->idx_qtree->totalObjects() << "\n";
	// this->idx_qtree->dfs(this->idx_qtree);
	return this->idx_qtree;
	
	// erebus::storage::qtree::Point start(-1, -1);
    // std::vector<erebus::storage::qtree::Point> pointsVector(100, start);
	// // this->idx_qtree = new erebus::storage::qtree::QuadTree(min_x, min_y, max_x, max_y, pointsVector);
	// erebus::storage::qtree::QuadTree tree(min_x, min_y, max_x, max_y, pointsVector);
	// for (int i = 0; i < 1000000; i++) {
	// 	double l, r, b, t;
	// 	ifs >> l >> r >> b >> t;
	// 	tree.insert(l, b);
	// }
	
	
	
	// for (int i = 0; i < 90000000; i++) {
	// 	double l, r, b, t;
	// 	ifs >> l >> r >> b >> t;
	// 	int data = i;
	// 	erebus::storage::qtree::Collidable obj = erebus::storage::qtree::Collidable({ l, r, 0, 0 }, data);
	// 	this->idx_qtree->insert(&obj);
	// }
	// std::cout << this->idx_qtree->totalChildren() << "\n";
	// std::cout << this->idx_qtree->totalObjects() << "\n";
	// ifs.close();
	// cout << this->idx->height_ << " " << GetIndexSizeInMB(this->idx) << endl;
	
	// Stuff about paging
	// int pgCnt = 10;
	// void *pgs[pgCnt];
	// for(auto i = 0; i < pgCnt; i++) pgs[i] = this->idx->tree_nodes_[i];
	// int status [pgCnt];
	// const int destNodes[pgCnt] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1};
	// int ret_code  = move_pages(0, pgCnt, pgs, destNodes, status, 0);

	// for(auto pnode = 0; pnode < pgCnt; pnode++){
	// 	void *ptr_to_check = this->idx->tree_nodes_[pnode];
	// 	int tstatus[1];
	// 	const int tDestNodes[1] = {destNodes[pnode]};
	// 	int tret_code = move_pages(0, 1, &ptr_to_check, tDestNodes, tstatus, 0);
	// 	printf("Memory at %p is at %d node (retcode %d)\n", ptr_to_check, tstatus[0], tret_code);    
		// void *pgs[9] = {
		// 	&(this->idx->tree_nodes_[pnode]->father),
		// 	&(this->idx->tree_nodes_[pnode]->children),
		// 	&(this->idx->tree_nodes_[pnode]->entry_num),
		// 	&(this->idx->tree_nodes_[pnode]->is_overflow),
		// 	&(this->idx->tree_nodes_[pnode]->is_leaf),
		// 	&(this->idx->tree_nodes_[pnode]->origin_center),
		// 	&(this->idx->tree_nodes_[pnode]->maximum_entry),
		// 	&(this->idx->tree_nodes_[pnode]->minimum_entry),
		// 	&(this->idx->tree_nodes_[pnode]->RR_s)
		// 			};
		
		// int pgStatus[9];
		// move_pages(0, 9, pgs, NULL, pgStatus, 0);
		// for(auto i = 0; i < 9; i++)
		// 	cout << pgStatus[i] << " ";
		// cout << endl;
		
		
		// // print all the nodes addresses
		// cout << "\t\t\t-------------------------------------------------------------------------------------" << endl;
		// for(auto cnode = 0; cnode <= pnode; cnode++){
		// 	void *cptr_to_check = this->idx->tree_nodes_[cnode];
		// 	int tcstatus[1];
			
		// 	int ctret_code = move_pages(0, 1, &cptr_to_check, NULL, tcstatus, 0);
		// 	printf("Memory at %p is at %d node (retcode %d)\n", cptr_to_check, tcstatus[0], ctret_code);    

		// }
		// cout << "\t\t\t-------------------------------------------------------------------------------------" << endl;
		
	// }
		
	
}

void Erebus::register_threadpool(erebus::tp::TPManager *tp)
{
	this->glb_tpool = tp;
}

}   // namespace erebus


int main()
{	
	// -------------------------------------------------------------------------------------

	// pcm::PCM * m = pcm::PCM::getInstance();

	// pcm::PCM * m2 = pcm::PCM::getInstance();

	// pcm::PCM::ErrorCode returnResult = m->program();
	// pcm::PCM::ErrorCode returnResult2 = m2->program();
	// if (returnResult != pcm::PCM::Success){
	// 	std::cerr << "Intel's PCM couldn't start" << std::endl;
	// 	std::cerr << "Error code: " << returnResult << std::endl;
	// 	exit(1);
	// }
	// if (returnResult2 != pcm::PCM::Success){
	// 	std::cerr << "Intel's PCM couldn't start" << std::endl;
	// 	std::cerr << "Error code: " << returnResult2 << std::endl;
	// 	exit(1);
	// }

	// pcm::SystemCounterState before_sstate = pcm::getSystemCounterState();
	
	/**
	 * These are my cores that I will be using, so they should not change.
	*/
	double min_x, max_x, min_y, max_y;
	#if DATASET == 0
		// ------------------------------US-NORTHEAST--------------------------------------------
		min_x = -83.478714;
		max_x = -65.87531;
		min_y = 38.78981;
		max_y = 47.491634;
	#elif DATASET == 1
		// ---------------------------GEOLIFE---------------------------------------------------
		min_x = -179.9695933;
		max_x = 179.9969416;
		min_y = 1.044024;
		max_y = 64.751993; // 200.166666666667
	#elif DATASET == 2
		// ---------------------------BMOD02---------------------------------------------------
		min_x = 1308;
		max_x = 12785;
		min_y = 1308;
		max_y = 12785; 
	#endif
	erebus::dm::GridManager glb_gm(10, 10, min_x, max_x, min_y, max_y);
	// -------------------------------------------------------------------------------------
	std::vector<CPUID> mm_cpuids;
	std::vector<CPUID> wrk_cpuids;
	std::vector<CPUID> rt_cpuids;
	std::vector<CPUID> ss_cpuids;
	std::vector<CPUID> ncore_cpuids;
	
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
	// int nWorkers = 7;  // Change the CURR_WORKER_THREADS in TPM.hpp
#if MACHINE == 0
	int nWorkers = 7;  // Change the CURR_WORKER_THREADS in TPM.hpp
#elif MACHINE == 1
	int nWorkers = 40;  // Change the CURR_WORKER_THREADS in TPM.hpp
#else
	int nWorkers = 7;  // Change the CURR_WORKER_THREADS in TPM.hpp
#endif
	
	ss_cpuids.push_back(99);
	
	for(auto n=0; n < nNUMANodes; n++){
		mm_cpuids.push_back(cPool[n][0]);
		
		rt_cpuids.push_back(cPool[n][1]);
		glb_gm.NUMAToRoutingCPUs.insert({n, cPool[n][1]});
		
		ncore_cpuids.push_back(cPool[n][2]);
		int cnt = 1;
		for(auto j = 3; j < cPool[n].size(); j++, cnt++){
			wrk_cpuids.push_back(cPool[n][j]);
			glb_gm.NUMAToWorkerCPUs.insert({n, cPool[n][j]});
			if (cnt == nWorkers) break;
		}
	}
	

	/**
	 * 
	 * */	
	
	erebus::scheduler::ResourceManager glb_rm;  // dummy, does not work at this point
	
	erebus::Erebus db(&glb_gm, &glb_rm);
	// WHICH INDEX?
	// -------------------------------------------------------------------------------------
	#if STORAGE == 0
		db.build_idx(1, 1);
		glb_gm.register_index(db.idx);
	#elif STORAGE ==1
		db.build_idx(min_x, max_x, min_y, max_y);
		glb_gm.register_index(db.idx_qtree);
	#endif
	// -------------------------------------------------------------------------------------	
	// erebus::storage::qtree::Rect bounds(
	// 	min_x,
	// 	min_y,
	// 	max_x - min_x,
	// 	max_y - min_y
	// );
	// int li_size = glb_gm.idx_quadtree->getObjectsInBound(bounds);
	// std::cout << li_size << endl;

	// glb_gm.register_grid_cells(wrk_cpuids);   // send the cpuids that can be used
	// From this point onwards repeat

	int cfgIdx = 901;
	glb_gm.register_grid_cells("/homes/yrayhan/works/erebus/src/config/machine-configs/config_" + std::to_string(cfgIdx) + ".txt");
	

	glb_gm.printGM();
	glb_gm.printQueryDistPushed();
	// #if STORAGE == 0
	// 	glb_gm.buildDataDistIdx();
	// #endif
	// glb_gm.printDataDistIdx();
	glb_gm.printDataDistIdxT();
	
	// WHICH INDEX?
	// -------------------------------------------------------------------------------------
	#if STORAGE == 0
		glb_gm.idx->NUMAStatus();
	#elif STORAGE ==1
		erebus::storage::qtree::NUMAstat ns;
		glb_gm.idx_quadtree->NUMAStatus(ns);
		for (int i =0; i < 8;i++){
			cout << ns.cntIndexNodes[i] << ' ';
		}
		cout << endl;	
	#endif

	// -------------------------------------------------------------------------------------
	
	// erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);
	erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);
	
	#if TEST_WORKLOAD_INTERFERENCE
		vector<CPUID> testCpuids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107};
		glb_tpool.testInterferenceInitWorkerThreads(testCpuids, testCpuids.size());
	#else 
		glb_tpool.initWorkerThreads();
		glb_tpool.initRouterThreads();
		glb_tpool.initSysSweeperThreads();
		glb_tpool.initMegaMindThreads();
		glb_tpool.initNCoreSweeperThreads();
		
	#endif 

	#if TEST_WORKLOAD_INTERFERENCE
		std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
		glb_tpool.terminateTestWorkerThreads();
		// std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		glb_tpool.dumpTestGridHWCounters(testCpuids);
	#endif
	
	std::this_thread::sleep_for(std::chrono::milliseconds(420000));  // 1000000 previously
	glb_tpool.terminateNCoreSweeperThreads();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	glb_tpool.dumpNCoreSweeperThreads();
	
	// glb_tpool.terminateRouterThreads();
	// std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	// glb_tpool.terminateWorkerThreads();
	// std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	// glb_tpool.terminateMegaMindThreads();
	// std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	// glb_tpool.terminateSysSweeperThreads();
	// std::this_thread::sleep_for(std::chrono::milliseconds(10000));

	// glb_tpool.glb_router_thrds.clear();
	// glb_tpool.glb_worker_thrds.clear();
	// glb_tpool.glb_sys_sweeper_thrds.clear();
	// glb_tpool.glb_megamind_thrds.clear();
	// glb_tpool.glb_ncore_sweeper_thrds.clear();
		

	
	
	while(1);

	// -------------------------------------------------------------------------------------

	// pcm::SystemCounterState after_sstate = pcm::getSystemCounterState();

	// std::cout << "Instructions per clock:" << pcm::getIPC(before_sstate,after_sstate) << std::endl;
	// std::cout << "Bytes read:" << pcm::getBytesReadFromMC(before_sstate,after_sstate) << std::endl; 
	// m->cleanup();
	// -------------------------------------------------------------------------------------


	
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



// int main1()
// {	

// 	// -------------------------------------------------------------------------------------

// 	// pcm::PCM * m = pcm::PCM::getInstance();

// 	// pcm::PCM * m2 = pcm::PCM::getInstance();

// 	// pcm::PCM::ErrorCode returnResult = m->program();
// 	// pcm::PCM::ErrorCode returnResult2 = m2->program();
// 	// if (returnResult != pcm::PCM::Success){
// 	// 	std::cerr << "Intel's PCM couldn't start" << std::endl;
// 	// 	std::cerr << "Error code: " << returnResult << std::endl;
// 	// 	exit(1);
// 	// }
// 	// if (returnResult2 != pcm::PCM::Success){
// 	// 	std::cerr << "Intel's PCM couldn't start" << std::endl;
// 	// 	std::cerr << "Error code: " << returnResult2 << std::endl;
// 	// 	exit(1);
// 	// }

// 	// pcm::SystemCounterState before_sstate = pcm::getSystemCounterState();
	
// 	/**
// 	 * These are my cores that I will be using, so they should not change.
// 	*/
// 	// ------------------------------US-NORTHEAST--------------------------------------------
// 	double min_x = -83.478714;
//     double max_x = -65.87531;
//     double min_y = 38.78981;
//     double max_y = 47.491634;
// 	erebus::dm::GridManager glb_gm(10, 10, min_x, max_x, min_y, max_y);
// 	// -------------------------------------------------------------------------------------
// 	// ---------------------------GEOLITE---------------------------------------------------
// 	// double min_x = -179.9695933;
//     // double max_x = 179.9969416;
//     // double min_y = 1.044024;
//     // double max_y = 400.166666666667;
// 	// erebus::dm::GridManager glb_gm(10, 10, min_x, max_x, min_y, max_y);
// 	// -------------------------------------------------------------------------------------
	

// 	std::vector<CPUID> mm_cpuids;
// 	std::vector<CPUID> wrk_cpuids;
// 	std::vector<CPUID> rt_cpuids;
// 	std::vector<CPUID> ss_cpuids;
// 	std::vector<CPUID> ncore_cpuids;
	
// 	int nNUMANodes = numa_num_configured_nodes();
// 	int nCPUCores = numa_num_possible_cpus();
	
// 	vector<CPUID> cPool[nNUMANodes];

// 	for(auto n=0; n < nNUMANodes; n++){
// 		struct bitmask *bmp = numa_allocate_cpumask();
// 		numa_node_to_cpus(n, bmp);
// 		for(auto j = 0; j < nCPUCores; j++){
// 			if (numa_bitmask_isbitset(bmp, j)) cPool[n].push_back(j);
// 		}
// 	}

// 	/**
// 	 * TODO: cleaner version would check the number of rt/numa node and allocate accordingly 
// 	 * Also would remove the number from the TPM.cc file about the #of threads and make it 
// 	 * global
// 	*/
// 	int nWorkers = 7;  // Change the CURR_WORKER_THREADS in TPM.hpp
	
// 	ss_cpuids.push_back(99);
	
// 	for(auto n=0; n < nNUMANodes; n++){
// 		mm_cpuids.push_back(cPool[n][0]);
		
// 		rt_cpuids.push_back(cPool[n][1]);
// 		glb_gm.NUMAToRoutingCPUs.insert({n, cPool[n][1]});
		
// 		ncore_cpuids.push_back(cPool[n][2]);
// 		int cnt = 1;
// 		for(auto j = 3; j < cPool[n].size(); j++, cnt++){
// 			wrk_cpuids.push_back(cPool[n][j]);
// 			glb_gm.NUMAToWorkerCPUs.insert({n, cPool[n][j]});
// 			if (cnt == nWorkers) break;
// 		}
// 	}
	

// 	/**
// 	 * 
// 	 * */	
	
// 	erebus::scheduler::ResourceManager glb_rm;  // dummy, does not work at this point
	
// 	erebus::Erebus db(&glb_gm, &glb_rm);
// 	db.build_idx(1, 1);
// 	glb_gm.register_index(db.idx);



// 	// glb_gm.register_grid_cells(wrk_cpuids);   // send the cpuids that can be used
// 	// From this point onwards repeat

// 	int cfgIdx = 1;
// 	glb_gm.register_grid_cells("/homes/yrayhan/works/erebus/src/config/machine-configs/config_" + std::to_string(cfgIdx) + ".txt");
	
	
	
// 	glb_gm.printGM();
// 	glb_gm.printQueryDistPushed();
// 	glb_gm.buildDataDistIdx();
// 	// glb_gm.printDataDistIdx();
// 	glb_gm.printDataDistIdxT();
// 	glb_gm.idx->NUMAStatus();
	
// 	// erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);
// 	erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);
	
// 	#if TEST_WORKLOAD_INTERFERENCE
// 		vector<CPUID> testCpuids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107};
// 		glb_tpool.testInterferenceInitWorkerThreads(testCpuids, testCpuids.size());
// 	#else 
// 		glb_tpool.initWorkerThreads();
// 		glb_tpool.initRouterThreads();
// 		glb_tpool.initSysSweeperThreads();
// 		glb_tpool.initMegaMindThreads();
// 		glb_tpool.initNCoreSweeperThreads();
		
// 	#endif 

// 	#if TEST_WORKLOAD_INTERFERENCE
// 		std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
// 		glb_tpool.terminateTestWorkerThreads();
// 		// std::this_thread::sleep_for(std::chrono::milliseconds(1000));
// 		glb_tpool.dumpTestGridHWCounters(testCpuids);
// 	#endif
	
// 	std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
// 	glb_tpool.terminateNCoreSweeperThreads();
// 	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
// 	glb_tpool.dumpNCoreSweeperThreads();
	
// 	// glb_tpool.terminateRouterThreads();
// 	// std::this_thread::sleep_for(std::chrono::milliseconds(10000));
// 	// glb_tpool.terminateWorkerThreads();
// 	// std::this_thread::sleep_for(std::chrono::milliseconds(10000));
// 	// glb_tpool.terminateMegaMindThreads();
// 	// std::this_thread::sleep_for(std::chrono::milliseconds(10000));
// 	// glb_tpool.terminateSysSweeperThreads();
// 	// std::this_thread::sleep_for(std::chrono::milliseconds(10000));

// 	// glb_tpool.glb_router_thrds.clear();
// 	// glb_tpool.glb_worker_thrds.clear();
// 	// glb_tpool.glb_sys_sweeper_thrds.clear();
// 	// glb_tpool.glb_megamind_thrds.clear();
// 	// glb_tpool.glb_ncore_sweeper_thrds.clear();
		

	
	
// 	while(1);

// 	// -------------------------------------------------------------------------------------

// 	// pcm::SystemCounterState after_sstate = pcm::getSystemCounterState();

// 	// std::cout << "Instructions per clock:" << pcm::getIPC(before_sstate,after_sstate) << std::endl;
// 	// std::cout << "Bytes read:" << pcm::getBytesReadFromMC(before_sstate,after_sstate) << std::endl; 
// 	// m->cleanup();
// 	// -------------------------------------------------------------------------------------


	
// 	while(1);
// 	return 0;

// 	// BS::thread_pool MegaMindPool(4);
// 	// BS::thread_pool WorkerPool(50);
// 	// auto mtids = MegaMindPool.get_thread_ids();
// 	// auto wtids = WorkerPool.get_thread_ids();

// 	// for (std::thread::id i: mtids) cout << i << ' '; cout << endl;
// 	// for (std::thread::id i: wtids) cout << i << ' '; cout << endl;

// 	// return 0;

// 	// tf::Executor executor;
//   	// tf::Taskflow taskflow;

// 	// auto [A, B, C, D] = taskflow.emplace(  // create four tasks
// 	// 	[] () { std::cout << "TaskA\n"; },
// 	// 	[] () { std::cout << "TaskB\n"; },
// 	// 	[] () { std::cout << "TaskC\n"; },
// 	// 	[] () { std::cout << "TaskD\n"; } 
// 	// );                                  
										
// 	// A.precede(B, C);  // A runs before B and C
// 	// D.succeed(B, C);  // D runs after  B and C
										
// 	// executor.run(taskflow).wait(); 

// 	// return 0;

//     // build_basertree(1, 1);

//     // unsigned num_cpus = std::thread::hardware_concurrency();
//     // std::cout << "Launching " << num_cpus << " threads\n";
    
//     // constexpr unsigned num_threads = 20;
//     // std::vector<std::thread> threads(num_threads);
//     // std::mutex iomutex;
    

// 	// test_loop_until_empty();
//     // test_init_thread();

   
// }