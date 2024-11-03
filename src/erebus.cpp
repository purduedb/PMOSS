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

Erebus::Erebus(erebus::dm::GridManager *gm, erebus::scheduler::ResourceManager *rm)
{	
	this->glb_gm = gm;
	this->glb_rm = rm;
}

erebus::storage::rtree::RTree* Erebus::build_rtree(int ds, int insert_strategy, int split_strategy) 
{
	this->idx = ConstructTree(50, 20);
	SetDefaultInsertStrategy(this->idx, insert_strategy);
	SetDefaultSplitStrategy(this->idx, split_strategy);
	int total_access = 0;
	
	ifstream ifs;
	int totPoints = 0;
	std::string ds_file = "/scratch1/yrayhan/dataset/";
	if (ds == OSM_USNE){
		ds_file += "us.txt";
		ifs.open(ds_file, std::ifstream::in); // 100000000
		totPoints = 50000000;
	}
	else if (ds == GEOLITE){
		ds_file += "geo.txt";
		ifs.open(ds_file, std::ifstream::in); // 24000000
		totPoints = 24000000;
	}
	else if (ds == BERLINMOD02){
		ds_file += "bmod02.txt";
		ifs.open(ds_file, std::ifstream::in);  //11975098
		totPoints = 11975098;
	}
	
	
	for (int i = 0; i < totPoints; i++) {
		double l, r, b, t;
		ifs >> l >> r >> b >> t;
		Rectangle* rectangle = InsertRec(this->idx, l, r, b, t);
		DefaultInsert(this->idx, rectangle);
	}
	ifs.close();
	cout << this->idx->height_ << " " << GetIndexSizeInMB(this->idx) << endl;	
	return this->idx;
}

erebus::storage::qtree::QuadTree* Erebus::build_idx(int ds, float min_x, float max_x, float min_y, float max_y) 
{
	ifstream ifs;
	int totPoints = 0;
	std::string ds_file = std::string(PROJECT_SOURCE_DIR) + "/src/dataset/";

	if (ds == OSM_USNE){
		ds_file += "us.txt";
		ifs.open(ds_file, std::ifstream::in); // 100000000
		totPoints = 90000000;
	}
	else if (ds == GEOLITE){
		ds_file += "geo.txt";
		ifs.open(ds_file, std::ifstream::in); // 24000000
		totPoints = 24000000;
	}
	else if (ds == BERLINMOD02){
		ds_file += "bmod02.txt";
		ifs.open(ds_file, std::ifstream::in);  //11975098
		totPoints = 11975098;
	}
	
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
  
	if (ds == YCSB) {
		init_file = "/scratch1/yrayhan/loade_zipf_int_200M.dat";
		// txn_file += "workloads/txnse_zipf_int_100M.dat";
  } else if (ds == WIKI) {
    init_file = "/scratch1/yrayhan/wiki_ts_200M_uint64.dat";
  } else if (ds == OSM_CELLIDS) {
    init_file = "/scratch1/yrayhan/osm_cellids_200M_uint64.dat";
  } 
	else {
    fprintf(stderr, "Unknown workload type or key type: %d, %d\n", ds, kt);
    exit(1);
  }

  std::ifstream infile_load(init_file);
	if(infile_load.is_open()){
		cout << "CHECKPOINT: FILE OPENED CORRECTLY" << endl;
	}
	else{
		cout << "CHECKPOINT FAILED!!!!: FILE DID NOT OPEN CORRECTLY" << endl;
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
	return this->idx_btree;


}
void Erebus::register_threadpool(erebus::tp::TPManager *tp)
{
	this->glb_tpool = tp;
}

} 


std::string get_cpu_vendor() {
    char cpu_vendor[13] = {0};  // Vendor string is 12 characters long + null terminator

#if defined(__GNUC__) || defined(__clang__)
    unsigned int eax, ebx, ecx, edx;
    // cpuid with eax=0 gives the vendor ID in ebx, edx, ecx
    __get_cpuid(0, &eax, &ebx, &ecx, &edx);
    std::memcpy(cpu_vendor + 0, &ebx, 4);  // EBX contains the first 4 characters
    std::memcpy(cpu_vendor + 4, &edx, 4);  // EDX contains the next 4 characters
    std::memcpy(cpu_vendor + 8, &ecx, 4);  // ECX contains the last 4 characters
#elif defined(_MSC_VER)
    int cpu_info[4] = {0};
    __cpuid(cpu_info, 0);  // CPUID function with eax=0
    std::memcpy(cpu_vendor + 0, &cpu_info[1], 4);  // EBX contains the first 4 characters
    std::memcpy(cpu_vendor + 4, &cpu_info[3], 4);  // EDX contains the next 4 characters
    std::memcpy(cpu_vendor + 8, &cpu_info[2], 4);  // ECX contains the last 4 characters
#endif

    return std::string(cpu_vendor);
}

int main(int argc, char* argv[])
{	
	
	int cfgIdx = 1;
	int ds = YCSB;
	int wl = SD_YCSB_WKLOADX2;
	int iam = BTREE;
	
	// int cfgIdx = 1;
	// int ds = OSM_USNE;
	// int wl = MD_RS_HOT7;
	// int iam = RTREE;

	if (argc > 1) {
		cfgIdx = std::atoi(argv[1]);
		wl = std::atoi(argv[2]);
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
	if (ds == OSM_USNE){  
		min_x = -83.478714; max_x = -65.87531; min_y = 38.78981; max_y = 47.491634;
	}
	else if (ds == GEOLITE){ 
		min_x = -179.9695933; max_x = 179.9969416; min_y = 1.044024; max_y = 64.751993; // 200.166666666667
	}
	else if (ds == BERLINMOD02){
		min_x = 1308; max_x = 12785; min_y = 1308; max_y = 12785; 
	}	
	else if (ds == YCSB){
		min_x = 36296660289; max_x = 9223371933865469581; min_y = -1; max_y = -1; 
		// min_x = 36296660289; max_x = 9223371992761358200; min_y = -1; max_y = -1; //100M and 200M Points and inserts
	}
	else if (ds == WIKI){
		// min_x = 979672113; max_x = 1216240436; min_y = -1; max_y = -1; // 200M points
		min_x = 979672113; max_x = 1173396408; min_y = -1; max_y = -1;  //100M points
	}
	else if (ds == OSM_CELLIDS){
		// min_x = 33246697004540789; max_x = 13748549577969753901; min_y = -1; max_y = -1;  //100M points
		min_x = 33246697004540789; max_x = 5170332552548576529; min_y = -1; max_y = -1;  //200M points
		// min_x = 33246697004540789; max_x = 13748551737189149045; min_y = -1; max_y = -1;  //800M points
		// min_x = 33246697004540789; max_x = 13748550930623082253; min_y = -1; max_y = -1;  //200M points
	}
	
#if MULTIDIM == 1
	erebus::dm::GridManager glb_gm(cfgIdx, wl, iam, MAX_XPAR, MAX_YPAR, min_x, max_x, min_y, max_y);
#else 
	erebus::dm::GridManager glb_gm(cfgIdx, wl, iam, MAX_GRID_CELL, 1, min_x, max_x, min_y, max_y);
#endif

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
		num_workers = 28;  
		ss_cpuids.push_back(74);
		mm_cpuids.push_back(75);
	#elif MACHINE == 2
		num_workers = 28;  
		ss_cpuids.push_back(31);
		mm_cpuids.push_back(63);
	#elif MACHINE == 5
		num_workers = 14;  
		ss_cpuids.push_back(74);
		mm_cpuids.push_back(75);
	#elif MACHINE == 6
		num_workers = 7;  // Change the CURR_WORKER_THREADS in TPM.hpp
		ss_cpuids.push_back(11);
		mm_cpuids.push_back(23);
	#else
		num_workers = 7;  
	#endif
	
	#if MACHINE == 6
	for(auto n=0; n < num_NUMA_nodes; n+=2){
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
	#else
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
	#endif 

	erebus::scheduler::ResourceManager glb_rm;  
	erebus::Erebus db(&glb_gm, &glb_rm);
	
	// -------------------------------------------------------------------------------------
	#if STORAGE == 0
		db.build_rtree(ds, 1, 1);
		glb_gm.register_index(db.idx);
	#elif STORAGE ==1
		db.build_idx(min_x, max_x, min_y, max_y);
		glb_gm.register_index(db.idx_qtree);
	#elif STORAGE == 2
		int kt = RAND_KEY;
		db.build_btree(ds, kt, init_keys, values);
		glb_gm.register_index(db.idx_btree);
	#endif
	
	std::string config_file;
	if(iam == BTREE){
	#if EVAL_PMOSS == 0
	#if MACHINE==0
		#if MAX_GRID_CELL == 100
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/skx_4s_8n/c_" + std::to_string(cfgIdx) + ".txt";
		#else 
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/skx_4s_8n/c_" + std::to_string(cfgIdx) + "_" + 
			std::to_string(MAX_GRID_CELL) + ".txt";
		#endif 
	#elif MACHINE==1
		#if MAX_GRID_CELL == 100
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/ice_2s_2n/c_" + std::to_string(cfgIdx) + ".txt";
		#endif
	#elif MACHINE==5
		#if MAX_GRID_CELL == 100
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/sb_4s_4n/c_" + std::to_string(cfgIdx) + ".txt";
		#else 
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/ice_2s_2n/c_" + std::to_string(cfgIdx) + "_" + 
			std::to_string(MAX_GRID_CELL) + ".txt";
		#endif 
	#elif MACHINE==6
		#if MAX_GRID_CELL == 100
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/skx_4s_4n/c_" + std::to_string(cfgIdx) + ".txt";
		#else 
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/skx_4s_4n/c_" + std::to_string(cfgIdx) + "_" + 
			std::to_string(MAX_GRID_CELL) + ".txt";
		#endif 
	#endif
	#else 
	#if MACHINE==0
		#if MAX_GRID_CELL == 100
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_8n/" + std::to_string(wl)
			+ "/c_" + std::to_string(cfgIdx) + ".txt";
		#else 
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_8n/" + std::to_string(wl)
			+ "/c_" + std::to_string(cfgIdx)+ "_" + std::to_string(MAX_GRID_CELL) + ".txt";
		#endif 
	#elif MACHINE==1
		#if MAX_GRID_CELL == 100
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_ice_2s_2n/" + std::to_string(wl)
			+ "/c_" + std::to_string(cfgIdx) + ".txt";
		#endif
	#elif MACHINE==6
		#if MAX_GRID_CELL == 100
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_4n/" + std::to_string(wl)
			+ "/c_" + std::to_string(cfgIdx) + ".txt";
		#else 
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_4n/" + std::to_string(wl)
			+ "/c_" + std::to_string(cfgIdx)+ "_" + std::to_string(MAX_GRID_CELL) + ".txt";
		#endif 
	#endif
	#endif
	}
	else if (iam == RTREE){
		#if EVAL_PMOSS == 0
		#if MACHINE==0
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/skx_4s_8n/c_" + std::to_string(cfgIdx) + "_" + 
				std::to_string(MAX_GRID_CELL) + "_r.txt";
		#endif
		#else 
		#if MACHINE==0	
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_8n/" + std::to_string(wl)
				+ "/c_" + std::to_string(cfgIdx)+ "_" + std::to_string(MAX_GRID_CELL) + "_r.txt";
		#endif
		#endif
	}

	cout << config_file << endl;
	glb_gm.register_grid_cells(config_file);
	glb_gm.buildDataDistIdx(iam, init_keys);
	glb_gm.printDataDistIdx();
	glb_gm.enforce_scheduling();
	#if STORAGE == 2
		db.idx_btree->count_numa_division(min_x, max_x, 100000);
	#elif STORAGE == 0
		glb_gm.idx->NUMAStatus();
	#endif
	glb_gm.printGM();

	// -------------------------------------------------------------------------------------
	
	erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);

	glb_tpool.init_worker_threads();
	glb_tpool.init_syssweeper_threads();
	glb_tpool.init_megamind_threads();
	glb_tpool.init_ncoresweeper_threads();
	glb_tpool.init_router_threads(ds, wl, min_x, max_x, min_y, max_y, init_keys, values);
	
	std::this_thread::sleep_for(std::chrono::milliseconds(300000));  //200000(ycsb-a), 490000, 1000000 previously
	glb_tpool.terminate_ncoresweeper_threads();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	glb_tpool.dump_ncoresweeper_threads();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	exit(0);
		
	while(1);

}



