#include "erebus.hpp"
// -------------------------------------------------------------------------------------
#include <iostream>
#include <fstream>
#include <thread>   // std::thread
#include <mutex>
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

erebus::storage::BTreeOLCIndex<keytype, keycomp>* Erebus::build_btree(const uint64_t ds, const uint64_t kt,  
	std::vector<keytype> &init_keys, std::vector<uint64_t> &values){
	
	this->idx_btree = new erebus::storage::BTreeOLCIndex<keytype, keycomp>(kt);

	std::vector<keytype> keys;
	std::vector<int> ranges;
	std::vector<int> ops; 
	int max_init_key = -1;
	static const uint64_t value_type=1; // 0 = random pointers, 1 = pointers to keys

	keys.reserve(10000000);
	ranges.reserve(10000000);
	ops.reserve(10000000);

	memset(&init_keys[0], 0x00, SINGLE_DIMENSION_KEY_LIMIT * sizeof(keytype));
	memset(&values[0], 0x00, SINGLE_DIMENSION_KEY_LIMIT * sizeof(uint64_t));
	memset(&keys[0], 0x00, 10000000 * sizeof(keytype));
	memset(&ranges[0], 0x00, 10000000 * sizeof(int));
	memset(&ops[0], 0x00, 10000000 * sizeof(int));

	std::string init_file = std::string(PROJECT_SOURCE_DIR) + "/src/";
  std::string txn_file = std::string(PROJECT_SOURCE_DIR) + "/src/";

	// init_file = "/mnt/nvme/";
	  
	if (ds == YCSB) {
		init_file += "dataset/loade_zipf_int_500M.dat";
  } 
	else if (ds == WIKI){
		init_file += "dataset/wiki_ts_200M_uint64.dat";
	}
	else if (ds == FB){
		init_file += "dataset/fb_200M_uint64.dat";
	}
	else if (ds == OSM_CELLIDS){
		init_file += "dataset/osm_cellids_600M_uint64.dat";
	}
	else {
    fprintf(stderr, "Unknown workload type or key type: %d, %d\n", ds, kt);
    exit(1);
  }

  std::ifstream infile_load(init_file);
	if(!infile_load.is_open()){
		cout << "CHECKPOINT FAILED!!!!\nFILE DID NOT OPEN CORRECTLY" << endl;
	}
	
  std::string op;
  keytype key;
  int range;

  std::string insert("INSERT");
  std::string read("READ");
  std::string update("UPDATE");
  std::string scan("SCAN");

  int count = 0;
  while ((count < SINGLE_DIMENSION_KEY_LIMIT)) {
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
    while (count < SINGLE_DIMENSION_KEY_LIMIT) {
      value = base + rand();
      values.push_back(value);
      count++;
    }
  }
  else {
    while (count < SINGLE_DIMENSION_KEY_LIMIT) {
      values.push_back(reinterpret_cast<uint64_t>(init_keys_data+count));
      count++;
    }
  }

	size_t total_num_key = init_keys.size();
	cout << total_num_key << endl;
	
	auto start = std::chrono::high_resolution_clock::now();
	for(size_t i = 0; i < BTREE_INIT_LIMIT; i++) {
		this->idx_btree->insert(init_keys[i], values[i]);
  }
	auto finish = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = finish - start;

	cout << "Checkpoint: INDEX_BUILD_COMPLETED: " << elapsed.count() << endl;
	
	// for(int i=0; i < total_num_key; i++){
	// 	// cout << i << ' ';
	// 	// cout << init_keys[i];
	// 	// cout << ' ';
	// 	cout << *reinterpret_cast<keytype*>(values[i]);
	// 	// cout << endl;
	// }
 
  // If we also execute transaction then open the 
  // transacton file here
	/*
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
	for(size_t i = 0;i < total_num_op; i++) {
		int op = ops[i];

		if (op == OP_INSERT) { //INSERT
			this->idx_btree->insert(keys[i], values[i]);
		}
		else if (op == OP_READ) { //READ
			cout << OP_READ << ' ' << i << ' ';
			v.clear();
			this->idx_btree->find(keys[i], &v);
			for(size_t j = 0; j < v.size(); j++) cout << v[j] << ' ';
			cout << endl;
		}
		else if (op == OP_UPSERT) { //UPDATE
			// this->idx_btree->upsert(keys[i], (uint64_t)keys[i].data);
		}
		else if (op == OP_SCAN) { //SCAN
			cout << keys[i] << ' ' << ranges[i] << "===> " ;
			cout << OP_SCAN << ' ' << i << ' ';
			int tem_result = this->idx_btree->scan(keys[i], ranges[i]);
			cout << tem_result << endl;
		}
		counter++;
	}
	*/
	return this->idx_btree;


}
void Erebus::register_threadpool(erebus::tp::TPManager *tp)
{
	this->glb_tpool = tp;
}

}   // namespace erebus


int main(int argc, char* argv[])
{	

	int cfgIdx = 300;
	int cfgIdxFuture = 301;
	int ds = YCSB;
	int wl = SD_YCSB_WKLOAD_MIGRATE1;
	int iam = BTREE;
	int migMode = 0;
	int migBatchSize = 256;
	double migRatio = 0.99;
	if (argc > 1) {
		// cfgIdx = std::atoi(argv[1]);
		wl = std::atoi(argv[1]);
		migMode = std::atoi(argv[2]);
		migBatchSize = std::atoi(argv[3]);
		migRatio = std::stod(argv[4]);
	}
	
	cout << "CONFIG=" << cfgIdx << endl;
	cout << "WKLOAD="  << wl << endl;
	

	// Keys in database 
	std::vector<keytype> init_keys;
	init_keys.reserve(SINGLE_DIMENSION_KEY_LIMIT);
	
	// Pointers to the keys
	std::vector<uint64_t> values;
	values.reserve(SINGLE_DIMENSION_KEY_LIMIT);
	
	double min_x, max_x, min_y, max_y;
	if (ds == YCSB){
		// min_x = 36296660289; max_x = 9223371933865469581; min_y = -1; max_y = -1; 
		//100M and 200M Points
		// min_x = 36296660289; max_x = 9223371992761358200; min_y = -1; max_y = -1; 
		//500M 
		min_x = 734139722786418736; max_x = 6075995071374232121; min_y = -1; max_y = -1; 
	}
	
	erebus::dm::GridManager glb_gm(cfgIdx, wl, iam, MAX_GRID_CELL, 1, min_x, max_x, min_y, max_y);

	// -------------------------------------------------------------------------------------
	std::vector<CPUID> mm_cpuids;  		// megamind cores
	std::vector<CPUID> wrk_cpuids;  	// worker cores
	std::vector<CPUID> rt_cpuids;  		// router cores
	std::vector<CPUID> ss_cpuids;			// memory channel sweeper cores
	std::vector<CPUID> ncore_cpuids;	// ncore sweeper cores
	
	int num_NUMA_nodes = numa_num_configured_nodes();
	int num_CPU_Cores = numa_num_possible_cpus();
	
	vector<CPUID> cPool[num_NUMA_nodes];

	for(auto n=0; n < num_NUMA_nodes; n++){
		struct bitmask *bmp = numa_allocate_cpumask();
		numa_node_to_cpus(n, bmp);
		for(auto j = 0; j < num_CPU_Cores; j++){
			if (numa_bitmask_isbitset(bmp, j)) cPool[n].push_back(j);
		}
	}

	int num_workers = 0;
	#if MACHINE == 0
		num_workers = 7;  // Change the CURR_WORKER_THREADS in TPM.hpp
		ss_cpuids.push_back(11);
		mm_cpuids.push_back(23);
	#elif MACHINE == 1
		num_workers = 40;  // Change the CURR_WORKER_THREADS in TPM.hpp
	#elif MACHINE == 2 
		// num_workers = 24;  // Change the CURR_WORKER_THREADS in TPM.hpp
		// ss_cpuids.push_back(31);
		// mm_cpuids.push_back(63);
		num_workers = 5;  // Change the CURR_WORKER_THREADS in TPM.hpp
		// ss_cpuids.push_back(31);
		// mm_cpuids.push_back(63);
	#elif MACHINE == 7
		num_workers = 14;  // Change the CURR_WORKER_THREADS in TPM.hpp
	#elif MACHINE == 3
		num_workers = 6;  // Change the CURR_WORKER_THREADS in TPM.hpp
	#else
		num_workers = 7;  // Change the CURR_WORKER_THREADS in TPM.hpp
	#endif
	
	#if MACHINE ==2 
		for(auto n=0; n < num_NUMA_nodes; n++){
			rt_cpuids.push_back(cPool[n][1]);
			glb_gm.NUMAToRoutingCPUs.insert({n, cPool[n][1]});
			
			ncore_cpuids.push_back(cPool[n][2]);
			
			int cnt = 1;
			for(size_t j = 3; j < cPool[n].size(); j++, cnt++){
				wrk_cpuids.push_back(cPool[n][j]);
				glb_gm.NUMAToWorkerCPUs.insert({n, cPool[n][j]});
				if (cnt == num_workers) break;
			}
		}
	#elif MACHINE==3 || MACHINE == 7
		for(auto n=0; n < num_NUMA_nodes; n++){
			rt_cpuids.push_back(cPool[n][0]);
			glb_gm.NUMAToRoutingCPUs.insert({n, cPool[n][0]});
			ncore_cpuids.push_back(cPool[n][1]);
			int cnt = 1;
			for(size_t j = 2; j < cPool[n].size(); j++, cnt++){
				wrk_cpuids.push_back(cPool[n][j]);
				glb_gm.NUMAToWorkerCPUs.insert({n, cPool[n][j]});
				if (cnt == num_workers) break;
			}
		}
	#endif 
	
	erebus::scheduler::ResourceManager glb_rm;  
	erebus::Erebus db(&glb_gm, &glb_rm);
	
	// -------------------------------------------------------------------------------------
	
	int kt = RAND_KEY;
	db.build_btree(ds, kt, init_keys, values);
	// Set the migration parameters
	db.idx_btree->migration_mode = migMode;
	db.idx_btree->bsize = migBatchSize;
	glb_gm.register_index(db.idx_btree);
	
	#if MACHINE == 2 
		#if MAX_GRID_CELL == 100
		std::string config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/amd_epyc7543_2s_2n/c_" + std::to_string(cfgIdx) + ".txt";
		#else 
		std::string config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/amd_epyc7543_2s_2n/c_" + std::to_string(cfgIdx) + "_" + 
		std::to_string(MAX_GRID_CELL) + ".txt";	
		std::string config_file_future = std::string(PROJECT_SOURCE_DIR) + "/src/config/amd_epyc7543_2s_2n/c_" + std::to_string(cfgIdxFuture) + "_" + 
		std::to_string(MAX_GRID_CELL) + ".txt";	
		#endif 
	#elif MACHINE == 7
		#if MAX_GRID_CELL == 100
		std::string config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/amd_epyc7302_2s_2n/c_" + std::to_string(cfgIdx) + ".txt";
		#else 
		std::string config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/amd_epyc7302_2s_2n/c_" + std::to_string(cfgIdx) + "_" + 
		std::to_string(MAX_GRID_CELL) + ".txt";	
		#endif 
	#elif MACHINE==3
		#if MAX_GRID_CELL == 100
		std::string config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/amd_epyc7543_2s_8n/c_" + std::to_string(cfgIdx) + ".txt";
		#else 
		std::string config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/amd_epyc7543_2s_8n/c_" + std::to_string(cfgIdx) + "_" + 
		std::to_string(MAX_GRID_CELL) + ".txt";	
		#endif 
	#endif
	
	glb_gm.register_grid_cells(config_file);
	// The config  file you want to migrate to
	glb_gm.register_grid_cells_future(config_file_future);
	glb_gm.buildDataDistIdx(iam, init_keys);
	// glb_gm.printDataDistIdx();
	
	glb_gm.enforce_scheduling();
	auto end = std::chrono::high_resolution_clock::now();
	
	
	db.idx_btree->count_numa_division(min_x, max_x, 100000);
	
	// glb_gm.printGM();
	
	
	erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);
	glb_tpool.migRatio = migRatio;
	glb_tpool.init_worker_threads();
	// glb_tpool.init_megamind_threads();
	glb_tpool.init_ncoresweeper_threads();
	glb_tpool.init_router_threads(ds, wl, min_x, max_x, min_y, max_y, init_keys, values);
	
	std::this_thread::sleep_for(std::chrono::milliseconds(360000));  // 200000(ycsb-a), 490000 (ini) 
	glb_tpool.terminate_ncoresweeper_threads();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	glb_tpool.dump_ncoresweeper_threads();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	glb_tpool.terminate_worker_threads();
	#if STORAGE == 2
		cout << db.idx_btree->migration_mode << " " << db.idx_btree->bsize  << " " << wl << " " << migRatio << endl;
		db.idx_btree->count_numa_division(min_x, max_x, 100000);
	#endif
	
	exit(0);		
	
	while(1);
}



