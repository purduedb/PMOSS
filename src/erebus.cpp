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
		init_file = "/scratch1/yrayhan/loade_zipf_int_1000M.dat";
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
	// Multi-threaded insertion
	std::vector<std::thread> insert_threads;
  int NUM_INSERTION_THREADS = erebus::tp::TPManager::CURR_WORKER_THREADS;
	insert_threads.reserve(NUM_INSERTION_THREADS);
	size_t chunk_size = BTREE_INIT_LIMIT / NUM_INSERTION_THREADS;
	size_t remainder = BTREE_INIT_LIMIT % NUM_INSERTION_THREADS;
	// Create worker threads
	for (unsigned i = 0; i < erebus::tp::TPManager::CURR_WORKER_THREADS; ++i) {
		// Determine the start and end indices for this thread
		size_t start_idx = i * chunk_size + std::min(static_cast<size_t>(i), remainder);
		size_t end_idx = start_idx + chunk_size + (i < remainder ? 1 : 0);
		// Ensure we don't exceed the array bounds
		if (start_idx >= BTREE_INIT_LIMIT) break;
		end_idx = std::min(end_idx, static_cast<size_t>(BTREE_INIT_LIMIT));

		// Launch a thread for the range [start_idx, end_idx)
		insert_threads.emplace_back([this, start_idx, end_idx, i, &init_keys, &values]() {
			// Pin the thread to the specified CPU
			erebus::utils::PinThisThread(i);
			// Small delay to avoid contention during thread startup
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			// Perform insertions for the assigned range
			for (size_t j = start_idx; j < end_idx; ++j) {
					this->idx_btree->insert(init_keys[j], values[j]);
			}
		});
	}
	// Wait for all threads to complete
	for (auto& th : insert_threads) {
			if (th.joinable()) {
					th.join();
			}
	}
	// for(size_t i = 0; i < BTREE_INIT_LIMIT; i++) {
	// 	this->idx_btree->insert(init_keys[i], values[i]);
  // }
	auto finish = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = finish - start;

	cout << "Checkpoint: INDEX_BUILD_COMPLETED: " << elapsed.count() << endl;
	return this->idx_btree;

}
void Erebus::register_threadpool(erebus::tp::TPManager *tp)
{
	this->glb_tpool = tp;
}

// -------------------------------------------------------------------------------------
// Dynamic Reconfiguration Methods
// -------------------------------------------------------------------------------------


std::string Erebus::generate_config_path(int config_id, int workload_id) {
	std::string config_file;
	int iam = this->glb_gm->iam;

	if (iam == BTREE) {
		#if EVAL_PMOSS == 0
		#if MACHINE==0
			#if MAX_GRID_CELL == 100
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/skx_4s_8n/c_" + std::to_string(config_id) + ".txt";
			#else
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/skx_4s_8n/c_" + std::to_string(config_id) + "_" +
				std::to_string(MAX_GRID_CELL) + ".txt";
			#endif
		#elif MACHINE==6
			#if MAX_GRID_CELL == 100
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/skx_4s_4n/c_" + std::to_string(config_id) + ".txt";
			#else
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/skx_4s_4n/c_" + std::to_string(config_id) + "_" +
				std::to_string(MAX_GRID_CELL) + ".txt";
			#endif
		#endif
		#else
		#if MACHINE==0
			#if MAX_GRID_CELL == 100
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_8n/" + std::to_string(workload_id)
				+ "/c_" + std::to_string(config_id) + ".txt";
			#else
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_8n/" + std::to_string(workload_id)
				+ "/c_" + std::to_string(config_id) + "_" + std::to_string(MAX_GRID_CELL) + ".txt";
			#endif
		#elif MACHINE==6
			#if MAX_GRID_CELL == 100
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_4n/" + std::to_string(workload_id)
				+ "/c_" + std::to_string(config_id) + ".txt";
			#else
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_4n/" + std::to_string(workload_id)
				+ "/c_" + std::to_string(config_id) + "_" + std::to_string(MAX_GRID_CELL) + ".txt";
			#endif
		#endif
		#endif
	} else if (iam == RTREE) {
		// R-tree config paths
		#if EVAL_PMOSS == 0
		#if MACHINE==0
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/skx_4s_8n/c_" + std::to_string(config_id) + "_" +
				std::to_string(MAX_GRID_CELL) + "_r.txt";
		#endif
		#else
		#if MACHINE==0
			config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_8n/" + std::to_string(workload_id)
				+ "/c_" + std::to_string(config_id) + "_" + std::to_string(MAX_GRID_CELL) + "_r.txt";
		#endif
		#endif
	}

	return config_file;
}

bool Erebus::perform_reconfiguration_dynamic(int new_config_id, int new_workload_id, int round) {
	std::lock_guard<std::mutex> lock(reconfig_state.reconfig_mutex);
	reconfig_state.is_reconfiguring = true;

	cout << "========================================" << endl;
	cout << "STARTING RECONFIGURATION" << endl;
	cout << "  Current Config: " << this->glb_gm->config << " -> New Config: " << new_config_id << endl;
	cout << "  Current Workload: " << this->glb_gm->wkload << " -> New Workload: " << new_workload_id << endl;
	cout << "========================================" << endl;

	auto reconfig_start = std::chrono::high_resolution_clock::now();


	// Step 0: Dump performance statistics from previous configuration
	cout << "[0/4] Dumping performance statistics for previous configuration..." << endl;
	auto step0_start = std::chrono::high_resolution_clock::now();
	this->glb_tpool->dump_ncoresweeper_threads(round);
	auto step0_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> step0_elapsed = step0_end - step0_start;
	cout << "  Stats dump completed in " << step0_elapsed.count() << " seconds" << endl;

	
	// Step 1: Update router threads with new workload (if changed)
	auto step1_start = std::chrono::high_resolution_clock::now();
	if (new_workload_id != this->glb_gm->wkload) {
		this->glb_tpool->pause_all_routers();
		cout << "[1/4] Synchronizing router thread workload change..." << endl;
		this->glb_tpool->initiate_workload_change(new_workload_id);
		this->glb_gm->wkload = new_workload_id;
		cout << "  Workload change completed" << endl;
	} else {
		cout << "[1/4] Workload unchanged, skipping router synchronization" << endl;
	}
	auto step1_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> step1_elapsed = step1_end - step1_start;
	cout << "  Step 1 completed in " << step1_elapsed.count() << " seconds" << endl;

	// Step 2: Migrate: Make sure routers and the workers are paused
	cout << "[2/4] Pausing all workers..." << endl;
	auto step2_start = std::chrono::high_resolution_clock::now();
	this->glb_tpool->pause_all_workers();
	// Add code for migration using enforce-schduling here
	
	auto step2_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> step2_elapsed = step2_end - step2_start;
	

	cout << "[3/4] Restarting all workers and routers" << endl;
	this->glb_tpool->resume_all_routers();
	this->glb_tpool->resume_all_workers();

	// Step 4: Update config ID and finalize
	cout << "[4/4] Finalizing reconfiguration..." << endl;
	this->glb_gm->config = new_config_id;

	auto reconfig_finish = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> total_elapsed = reconfig_finish - reconfig_start;

	reconfig_state.is_reconfiguring = false;

	return true;
}




bool Erebus::perform_reconfiguration_static(int new_config_id, int new_workload_id, int round) {
	cout << "========================================" << endl;
	cout << "STARTING RECONFIGURATION" << endl;
	cout << "  Current Config: " << this->glb_gm->config << " -> New Config: " << new_config_id << endl;
	cout << "  Current Workload: " << this->glb_gm->wkload << " -> New Workload: " << new_workload_id << endl;
	cout << "========================================" << endl;

	auto reconfig_start = std::chrono::high_resolution_clock::now();

	// Step 0: Dump performance statistics from previous configuration
	cout << "[0/2] Dumping performance statistics for previous configuration..." << endl;
	auto step0_start = std::chrono::high_resolution_clock::now();
	this->glb_tpool->dump_ncoresweeper_threads(round);
	auto step0_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> step0_elapsed = step0_end - step0_start;
	
	std::lock_guard<std::mutex> lock(reconfig_state.reconfig_mutex);
	reconfig_state.is_reconfiguring = true;

	// Step 1: Update router threads with new workload (if changed)
	auto step1_start = std::chrono::high_resolution_clock::now();
	// Can we move this after the checking if the new workload has been already changed or not
	if (new_workload_id != this->glb_gm->wkload) {
		this->glb_tpool->pause_all_routers();
		cout << "[1/2] Initiate router thread workload change..." << endl;
		this->glb_tpool->initiate_workload_change(new_workload_id);
		this->glb_gm->wkload = new_workload_id;
		cout << "[1/2] Workload change completed" << endl;
	} else {
		cout << "[1/2] Workload unchanged, skipping router synchronization" << endl;
	}
	this->glb_tpool->resume_all_routers();

	auto step1_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> step1_elapsed = step1_end - step1_start;
	
	// Step 4: Update config ID and finalize
	cout << "[2/2] Finalizing reconfiguration..." << endl;
	this->glb_gm->config = new_config_id;

	auto reconfig_finish = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> total_elapsed = reconfig_finish - reconfig_start;

	reconfig_state.is_reconfiguring = false;

	cout << "========================================" << endl;
	cout << "Reconfiguration completed in " << total_elapsed.count() << " seconds" << endl;
	cout << "========================================" << endl;
	return true;
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
	
	auto start = std::chrono::high_resolution_clock::now();
	int cfgIdx = 1;
	int ds = YCSB;
	int wl = SD_YCSB_WKLOADA;
	int iam = BTREE;
	int round = 0;
	if (argc > 1) {
		cfgIdx = std::atoi(argv[1]);
		wl = std::atoi(argv[2]);
		round = std::atoi(argv[3]);
	}
	
	cout << "CONFIG=" << cfgIdx << endl;
	cout << "WKLOAD="  << wl << endl;
	cout << "ROUND="   << round << endl;

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
		// min_x = 36296660289; max_x = 		9223371933865469581; min_y = -1; max_y = -1; 
		// min_x = 36296660289; max_x = 9223371992761358200; min_y = -1; max_y = -1; //100M and 200M Points and inserts
		//500M 
		min_x = 734139722786418736; max_x = 6075995071374232121; min_y = -1; max_y = -1; 
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
	#if MACHINE == 0 						// BIGDATA
		num_workers = 10; 					// Change the CURR_WORKER_THREADS in TPM.hpp
		ss_cpuids.push_back(0);
		mm_cpuids.push_back(12);
	#elif MACHINE == 6
		num_workers = 10;  
		ss_cpuids.push_back(0);
		mm_cpuids.push_back(24);
	#else
		num_workers = 7;  
	#endif
	
	#if MACHINE == 0				
	for(auto n=0; n < num_NUMA_nodes; n++){
		rt_cpuids.push_back(cPool[n][1]);
		glb_gm.NUMAToRoutingCPUs.insert({n, cPool[n][1]});
		
		ncore_cpuids.push_back(cPool[n][2]);
		
		int cnt = 0;
		for(size_t j = 0; j < cPool[n].size(); j++){
			if (j == 1 || j == 2) 
				continue;
			if (cPool[n][j] == 0 || cPool[n][j] == 12){
				cnt++;
				continue; 
			} 
			wrk_cpuids.push_back(cPool[n][j]);
			glb_gm.NUMAToWorkerCPUs.insert({n, cPool[n][j]});
			cnt++;
			if (cnt == num_workers) break;
		}
	}
	#elif MACHINE == 6
		for(auto n=0; n < num_NUMA_nodes; n+=2){
			rt_cpuids.push_back(cPool[n][1]);
			glb_gm.NUMAToRoutingCPUs.insert({n, cPool[n][1]});
			ncore_cpuids.push_back(cPool[n][2]);
			int cnt = 0;
			for(size_t j = 0; j < cPool[n].size(); j++){
				if (j == 1 || j == 2) 
					continue;
				if (cPool[n][j] == 0 || cPool[n][j] == 24){
					cnt++;
					continue; 
				} 
				wrk_cpuids.push_back(cPool[n][j]);
				glb_gm.NUMAToWorkerCPUs.insert({n, cPool[n][j]});
				cnt++;
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
	// glb_gm.printDataDistIdx();
	glb_gm.enforce_scheduling();
	
	// #if STORAGE == 2
	// 	db.idx_btree->count_numa_division(min_x, max_x, 100000);
	// #elif STORAGE == 0
	// 	glb_gm.idx->NUMAStatus();
	// #endif
	glb_gm.printGM();

	// -------------------------------------------------------------------------------------
	
	erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);

	glb_tpool.init_worker_threads();
	glb_tpool.init_syssweeper_threads();
	glb_tpool.init_ncoresweeper_threads();
	glb_tpool.init_router_threads(ds, wl, min_x, max_x, min_y, max_y, init_keys, values);
	db.register_threadpool(&glb_tpool);

	std::vector<std::pair<int, int>> workload_config_sequence = {
		{SD_YCSB_WKLOADC, cfgIdx},
		{SD_YCSB_WKLOADH, 204},
		{SD_YCSB_WKLOADA, 200},
	};
	int current_sequence_index = 0;  // Start at index 0 (initial workload/config)

	const int RUN_DURATION_MS = 900000; // 900 seconds per workload
	const int CHECK_INTERVAL_MS = 5000; // Check every 5 seconds
	const int MAX_PASSES = 1; // Number of complete passes through the workload sequence before terminating

	bool keep_running = true;
	int iteration_count = 0;
	int workload_run_count = 0;
	int sequence_passes = 0;  // Track complete passes through the workload_config_sequence
	
	cout << "========================================" << endl;
	cout << "P-MOSS: Continuous Execution Mode" << endl;
	cout << "  Run duration per workload: " << RUN_DURATION_MS/1000 << " seconds" << endl;
	cout << "  Initial configuration: Workload " << wl << ", Config " << cfgIdx << endl;
	cout << "  Maximum passes through sequence: " << MAX_PASSES << endl;
	cout << "  Workload-Config sequence: ";
	for (size_t i = 0; i < workload_config_sequence.size(); i++) {
		cout << "[W" << workload_config_sequence[i].first << ",C" << workload_config_sequence[i].second << "]";
		if (i < workload_config_sequence.size() - 1) cout << " -> ";
	}
	cout << endl;
	cout << "========================================" << endl;
	
	
	
	auto run_start_time = std::chrono::high_resolution_clock::now();
	
	while (keep_running) {
		std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
		iteration_count++;
		
		// Check if run duration completed
		auto current_time = std::chrono::high_resolution_clock::now();
		auto elapsed_since_run_start = std::chrono::duration_cast<std::chrono::milliseconds>(
			current_time - run_start_time).count();
		
		if (elapsed_since_run_start >= RUN_DURATION_MS) {
			workload_run_count++;
			
			cout << "========================================" << endl;
			cout << "RUN #" << workload_run_count << " COMPLETED" << endl;
			cout << "  Duration: " << elapsed_since_run_start/1000 << " seconds" << endl;
			cout << "  Current workload: " << glb_gm.wkload << endl;
			cout << "  Current config: " << glb_gm.config << endl;
			cout << "========================================" << endl;

			// Determine next workload and config in sequence
			current_sequence_index = (current_sequence_index + 1) % workload_config_sequence.size();

			// Check if we've completed a full pass through the sequence
			if (current_sequence_index == 0) {
				sequence_passes++;
				cout << "Completed pass #" << sequence_passes << " through workload sequence" << endl;

				// Check if we've reached the maximum number of passes
				if (sequence_passes >= MAX_PASSES) {
					cout << "Reached maximum passes (" << MAX_PASSES << "). Initiating graceful shutdown..." << endl;
					keep_running = false;
					continue; // Skip reconfiguration and exit the loop
				}
			}

			int next_workload = workload_config_sequence[current_sequence_index].first;
			int next_config = workload_config_sequence[current_sequence_index].second;

			cout << "Initiating change to: Workload " << next_workload << ", Config " << next_config;
			cout << " (Sequence index: " << current_sequence_index << ", Pass: " << (sequence_passes + 1) << ")" << endl;

			// Trigger reconfiguration with paired workload and config
			// If ENABLE_DYNAMIC_RECONFIGURATION is set, use dynamic else use static
			bool success = false;
			success = db.perform_reconfiguration_static(next_config, next_workload, round);
			
			if (success) {
				cout << "Reconfiguration successful. Continuing with next run..." << endl;
			} else {
				cout << "Reconfiguration failed. Keeping current configuration." << endl;
			}
			
			// How to make sure this if code is executed but the loop continues on, its just that the system 
			// will figure that there is a workload change and do some other stuff here in the background and then
			// call the dynamic function
			
			if (ENABLE_DYNAMIC_RECONFIGURATION) {
				string next_config_path = db.generate_config_path(next_config, next_workload);
				cout << "Dynamic reconfiguration to: Workload " << next_workload << ", Config" << next_config << endl;
				glb_tpool.init_megamind_threads(next_config, next_workload, next_config_path); // Ensure megamind is running
				
			} 
			// Reset run timer
			run_start_time = std::chrono::high_resolution_clock::now();
		}
	}
	
	// -------------------------------------------------------------------------------------
	// Graceful Shutdown Sequence
	// -------------------------------------------------------------------------------------
	
	cout << "========================================" << endl;
	cout << "SHUTTING DOWN P-MOSS" << endl;
	cout << "========================================" << endl;
	
	// Stop generating queries
	cout << "[1/5] Terminating router threads..." << endl;
	glb_tpool.terminate_router_threads();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	
	// Let workers finish current queries
	cout << "[2/5] Terminating worker threads..." << endl;
	glb_tpool.terminate_worker_threads();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	
	// Stop profiling
	cout << "[3/5] Terminating sweeper threads..." << endl;
	glb_tpool.terminate_ncoresweeper_threads();
	glb_tpool.terminate_syssweeper_threads();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	
	// Dump final statistics
	cout << "[4/5] Dumping final performance statistics..." << endl;
	glb_tpool.dump_ncoresweeper_threads(round);
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	
	// Print final summary
	cout << "[5/5] Generating final summary..." << endl;
	
	cout << "========================================" << endl;
	cout << "P-MOSS SHUTDOWN COMPLETE" << endl;
	cout << "  Total iterations: " << iteration_count << endl;
	cout << "  Workload runs completed: " << workload_run_count << endl;
	cout << "  Complete sequence passes: " << sequence_passes << "/" << MAX_PASSES << endl;
	cout << "========================================" << endl;
	
	return 0;
}



