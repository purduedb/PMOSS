#include "erebus.hpp"
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
	
	return this->idx_qtree;	
}


erebus::storage::BTreeOLCIndex<keytype, keycomp>* Erebus::build_btree(const uint64_t wl, const uint64_t kt){
	this->idx_btree = new erebus::storage::BTreeOLCIndex<keytype, keycomp>(kt);
	
	cout << "====== inside b tree function" << endl;
	std::vector<keytype> init_keys;
	std::vector<keytype> keys;
	std::vector<uint64_t> values;
	std::vector<int> ranges;
	std::vector<int> ops; //INSERT = 0, READ = 1, UPDATE = 2
	int max_init_key = -1;
	static const uint64_t value_type=1; // 0 = random pointers, 1 = pointers to keys

	init_keys.reserve(50000000);
	keys.reserve(10000000);
	values.reserve(10000000);
	ranges.reserve(10000000);
	ops.reserve(10000000);

	memset(&init_keys[0], 0x00, 50000000 * sizeof(keytype));
	memset(&keys[0], 0x00, 10000000 * sizeof(keytype));
	memset(&values[0], 0x00, 10000000 * sizeof(uint64_t));
	memset(&ranges[0], 0x00, 10000000 * sizeof(int));
	memset(&ops[0], 0x00, 10000000 * sizeof(int));

	std::string init_file;
  std::string txn_file;

  if (kt == RAND_KEY && wl == WORKLOAD_A) {
    init_file = "/homes/yrayhan/works/erebus/src/workloads/loada_zipf_int_100M.dat";
    txn_file = "/homes/yrayhan/works/erebus/src/workloads/txnsa_zipf_int_100M.dat";
  } else if (kt == RAND_KEY && wl == WORKLOAD_C) {
    init_file = "/homes/yrayhan/works/erebus/src/workloads/loadc_zipf_int_100M.dat";
    txn_file = "/homes/yrayhan/works/erebus/src/workloads/txnsc_zipf_int_100M.dat";
  } else if (kt == RAND_KEY && wl == WORKLOAD_E) {
    init_file = "/homes/yrayhan/works/erebus/src/workloads/loade_zipf_int_100M.dat";
    txn_file = "/homes/yrayhan/works/erebus/src/workloads/txnse_zipf_int_100M.dat";
  } else if (kt == MONO_KEY && wl == WORKLOAD_A) {
    init_file = "/homes/yrayhan/works/erebus/src/workloads/mono_inc_loada_zipf_int_100M.dat";
    txn_file = "/homes/yrayhan/works/erebus/src/workloads/mono_inc_txnsa_zipf_int_100M.dat";
  } else if (kt == MONO_KEY && wl == WORKLOAD_C) {
    init_file = "/homes/yrayhan/works/erebus/src/workloads/mono_inc_loadc_zipf_int_100M.dat";
    txn_file = "/homes/yrayhan/works/erebus/src/workloads/mono_inc_txnsc_zipf_int_100M.dat";
  } else if (kt == MONO_KEY && wl == WORKLOAD_E) {
    init_file = "/homes/yrayhan/works/erebus/src/workloads/mono_inc_loade_zipf_int_100M.dat";
    txn_file = "/homes/yrayhan/works/erebus/src/workloads/mono_inc_txnse_zipf_int_100M.dat";
  } else {
    fprintf(stderr, "Unknown workload type or key type: %d, %d\n", wl, kt);
    exit(1);
  }

	cout << "====== loading the index data files" << endl;
  std::ifstream infile_load(init_file);

  std::string op;
  keytype key;
  int range;

  std::string insert("INSERT");
  std::string read("READ");
  std::string update("UPDATE");
  std::string scan("SCAN");

  int count = 0;
  while ((count < INIT_LIMIT)) {
    infile_load >> op >> key;
    if (op.compare(insert) != 0) {
      std::cout << "READING LOAD FILE FAIL!\n";
      break;
    }
    init_keys.push_back(key);
    count++;

    // If we have reached the max init key limit then just break
    if(max_init_key > 0 && count == max_init_key) {
      break;
    }
  }
  
  fprintf(stderr, "Loaded %d keys\n", count);

  count = 0;
  uint64_t value = 0;
  void *base_ptr = malloc(8);
  uint64_t base = (uint64_t)(base_ptr);
  free(base_ptr);

  keytype *init_keys_data = init_keys.data();

  if (value_type == 0) {
    while (count < INIT_LIMIT) {
      value = base + rand();
      values.push_back(value);
      count++;
    }
  }
  else {
    while (count < INIT_LIMIT) {
      values.push_back(reinterpret_cast<uint64_t>(init_keys_data+count));
      count++;
    }
  }


	size_t total_num_key = init_keys.size();
	cout << total_num_key << endl;
	for(size_t i = 0; i < total_num_key; i++) {
		this->idx_btree->insert(init_keys[i], values[i]);
		// cout << init_keys[i] << " " << values[i] << endl;
  }

  // If we also execute transaction then open the 
  // transacton file here
  std::ifstream infile_txn(txn_file);
  
  count = 0;
  while ((count < LIMIT) && infile_txn.good()) {
    infile_txn >> op >> key;
    if (op.compare(insert) == 0) {
      ops.push_back(OP_INSERT);
      keys.push_back(key);
      ranges.push_back(1);
    }
    else if (op.compare(read) == 0) {
      ops.push_back(OP_READ);
      keys.push_back(key);
    }
    else if (op.compare(update) == 0) {
      ops.push_back(OP_UPSERT);
      keys.push_back(key);
    }
    else if (op.compare(scan) == 0) {
      infile_txn >> range;
      ops.push_back(OP_SCAN);
      keys.push_back(key);
      ranges.push_back(range);
    }
    else {
      std::cout << "UNRECOGNIZED CMD!\n";
      break;
    }
    count++;
  }


  // Average and variation
  long avg = 0, var = 0;
  // If it is YSCB-E workload then we compute average and stdvar
  if(ranges.size() != 0) {
    for(int r : ranges) {
      avg += r;
    }

    avg /= (long)ranges.size();

    for(int r : ranges) {
      var += ((r - avg) * (r - avg));
    }

    var /= (long)ranges.size();

    fprintf(stderr, "YCSB-E scan Avg length: %ld; Variance: %ld\n",
            avg, var);
  }

	size_t total_num_op = ops.size();
	
	std::vector<uint64_t> v;
	v.reserve(10);

    
	int counter = 0;
	for(size_t i = 0;i < total_num_op;i++) {
		int op = ops[i];

		if (op == OP_INSERT) { //INSERT
			this->idx_btree->insert(keys[i], values[i]);
		}
		else if (op == OP_READ) { //READ
			v.clear();
			this->idx_btree->find(keys[i], &v);
		}
		else if (op == OP_UPSERT) { //UPDATE
			// this->idx_btree->upsert(keys[i], (uint64_t)keys[i].data);
		}
		else if (op == OP_SCAN) { //SCAN
			this->idx_btree->scan(keys[i], ranges[i]);
		}
		counter++;
	}

	return this->idx_btree;


}
void Erebus::register_threadpool(erebus::tp::TPManager *tp)
{
	this->glb_tpool = tp;
}

}   // namespace erebus


int main()
{	
	
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

#if MULTIDIM == 1
	erebus::dm::GridManager glb_gm(10, 10, min_x, max_x, min_y, max_y);
#else 
	erebus::dm::GridManager glb_gm(100, 1, min_x, max_x, min_y, max_y);
#endif

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
#elif MACHINE == 3
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
	#elif STORAGE == 2
		int kt = RAND_KEY;
		int wl = WORKLOAD_E;
		db.build_btree(wl, kt);
		glb_gm.register_index(db.idx_btree);
	#endif


	int cfgIdx = 500001;
	glb_gm.register_grid_cells("/homes/yrayhan/works/erebus/src/config/machine-configs/config_" + std::to_string(cfgIdx) + ".txt");
	// glb_gm.register_grid_cells("/homes/yrayhan/works/erebus/src/config/l-machine-configs/config_" + std::to_string(cfgIdx) + ".txt");
	

	glb_gm.printGM();
	glb_gm.printQueryDistPushed();
	// #if STORAGE == 0
	// 	glb_gm.buildDataDistIdx();
	// #endif
	// glb_gm.printDataDistIdx();
	glb_gm.printDataDistIdxT();
	
	// WHICH INDEX?
	// -------------------------------------------------------------------------------------
	// #if STORAGE == 0
	// 	glb_gm.idx->NUMAStatus();
	// #elif STORAGE ==1
	// 	erebus::storage::qtree::NUMAstat ns;
	// 	glb_gm.idx_quadtree->NUMAStatus(ns);
	// 	for (int i =0; i < 8;i++){
	// 		cout << ns.cntIndexNodes[i] << ' ';
	// 	}
	// 	cout << endl;	
	// #endif

	// -------------------------------------------------------------------------------------
	
	// erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);
	erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);
	
	glb_tpool.initWorkerThreads();
	glb_tpool.initRouterThreads();
	glb_tpool.initSysSweeperThreads();
	glb_tpool.initMegaMindThreads();
	glb_tpool.initNCoreSweeperThreads();
		
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
	
}



