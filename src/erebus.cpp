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

erebus::storage::rtree::RTree* Erebus::build_rtree(int ds, int insert_strategy, int split_strategy) 
{
	this->idx = ConstructTree(50, 20);
	SetDefaultInsertStrategy(this->idx, insert_strategy);
	SetDefaultSplitStrategy(this->idx, split_strategy);
	int total_access = 0;
	
	ifstream ifs;
	int totPoints = 0;
	std::string ds_file = std::string(PROJECT_SOURCE_DIR) + "/src/dataset/";

	if (ds == OSM_USNE){
		ds_file = "/scratch1/yrayhan/dataset/";
		ds_file += "us.txt";
		ifs.open(ds_file, std::ifstream::in); // 100000000
		totPoints = 50000000;
	}
	else if (ds == GEOLITE){
		ds_file = "/scratch1/yrayhan/dataset/";
		ds_file += "geo.txt";
		ifs.open(ds_file, std::ifstream::in); // 24000000
		totPoints = 24000000;
	}
	else if (ds == BERLINMOD02){
		ds_file = "/scratch1/yrayhan/dataset/";
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
	
	#if MACHINE==0
		init_file = "/scratch1/yrayhan/";
	#elif MACHINE==1
		init_file = "/home/yrayhan/works/erebus/src/workloads/";
	#elif MACHINE==2
		init_file;
	#elif MACHINE==3
		init_file;
	#elif MACHINE==4
		init_file;
	#elif MACHINE==5
		init_file;
	#endif 
	
	  
	if (ds == YCSB) {
		#if MACHINE==0
		init_file += "loade_zipf_int_200M.dat";
		#else
		init_file += "dataset/loade_zipf_int_200M.dat";
		#endif		
  	}  
	else if (ds == WIKI){
		init_file += "dataset/wiki_ts_200M_uint64.dat";
	}
	else if (ds == FB){
		init_file += "dataset/fb_200M_uint64.dat";
	}
	else if (ds == OSM_CELLIDS){
		// init_file += "dataset/osm_cellids_200M_uint64.dat";
		init_file += "osm_cellids_200M_uint64.dat";
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

}   


int main(int argc, char* argv[])
{	
	// int cfgIdx = 506;
	// int ds = YCSB;
	// int wl = SD_YCSB_WKLOADH;
	// int iam = BTREE;

	int cfgIdx = 1;
	int ds = OSM_USNE;
	int wl = MD_RS_HOT7;
	int iam = RTREE;

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
		// min_x = 36296660289; max_x = 9223371933865469581; min_y = -1; max_y = -1; 
		min_x = 36296660289; max_x = 9223371992761358200; min_y = -1; max_y = -1; //100M and 200M Points and inserts
	}
	else if (ds == WIKI){
		// min_x = 979672113; max_x = 1216240436; min_y = -1; max_y = -1; // 200M points
		min_x = 979672113; max_x = 1173396408; min_y = -1; max_y = -1;  //100M points
	}
	else if (ds == FB){
		min_x = 1; max_x = 18446744073709551615; min_y = -1; max_y = -1; 
	}
	else if (ds == OSM_CELLIDS){
		// min_x = 33246697004540789; max_x = 13748549577969753901; min_y = -1; max_y = -1;  	//100M points
		min_x = 33246697004540789; max_x = 5170332552548576529; min_y = -1; max_y = -1;  			//200M points
		// min_x = 33246697004540789; max_x = 13748551737189149045; min_y = -1; max_y = -1;  	//800M points
		// min_x = 33246697004540789; max_x = 13748550930623082253; min_y = -1; max_y = -1;  	//200M points
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
	std::string machine_name;
	#if MACHINE == 0
		num_workers = 7;  // Change the CURR_WORKER_THREADS in TPM.hpp
		machine_name = "intel_skx_4s_8n";
		ss_cpuids.push_back(11);
	#elif MACHINE == 1
		num_workers = 28;  
		machine_name = "intel_ice_2s_2n";
	#elif MACHINE == 2
		num_workers = 28;  // Change the CURR_WORKER_THREADS in TPM.hpp
		machine_name = "amd_epyc7543_2s_2n";
	#elif MACHINE == 3
		num_workers = 6;  // Change the CURR_WORKER_THREADS in TPM.hpp	
		machine_name = "amd_epyc7543_2s_8n";
	#elif MACHINE == 4
		num_workers = 56;  // Change the CURR_WORKER_THREADS in TPM.hpp	
		machine_name = "nvidia_gh_1s_1n";
	#elif MACHINE == 5
		num_workers = 10;  
		machine_name = "intel_sb_4s_4n";
	#elif MACHINE == 6
		num_workers = 7;  // Change the CURR_WORKER_THREADS in TPM.hpp
		machine_name = "intel_skx_4s_4n";
	#elif MACHINE == 7
		num_workers = 14;  // Change the CURR_WORKER_THREADS in TPM.hpp
	#endif
	
	#if MACHINE==3 || MACHINE == 7
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
	#elif MACHINE==4 
		for(auto n=0; n < 1; n++){
			rt_cpuids.push_back(cPool[n][0]);
			glb_gm.NUMAToRoutingCPUs.insert({n, cPool[n][0]});
			ncore_cpuids.push_back(cPool[n][1]);
			int cnt = 1;
			for(size_t j = 3; j < cPool[n].size(); j++, cnt++){
				wrk_cpuids.push_back(cPool[n][j]);
				glb_gm.NUMAToWorkerCPUs.insert({n, cPool[n][j]});
				if (cnt == num_workers) break;
			}
		}
	#elif MACHINE == 6
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
		#if MAX_GRID_CELL == 100
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/";
		config_file += machine_name;
		config_file += "/c_" + std::to_string(cfgIdx) + ".txt";
		#else
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/";
		config_file += machine_name;
		config_file += "/c_" + std::to_string(cfgIdx) + "_";
		config_file += std::to_string(MAX_GRID_CELL) + 
		".txt";
		#endif
	}
	else if (iam == RTREE){
		#if MAX_GRID_CELL == 100
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/";
		config_file += machine_name;
		config_file += "/c_" + std::to_string(cfgIdx) + ".txt";
		#else
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/config/";
		config_file += machine_name;
		config_file += "/c_" + std::to_string(cfgIdx) + "_";
		config_file += std::to_string(MAX_GRID_CELL) + 
		".txt";
		#endif
	}
	
	cout << config_file << endl;

	glb_gm.register_grid_cells(config_file);
	if (cfgIdx > 501){
		glb_gm.buildDataDistIdx(iam, init_keys);
		glb_gm.printDataDistIdx();
		glb_gm.enforce_scheduling();
	}
		
	
	#if STORAGE == 2
		db.idx_btree->count_numa_division(min_x, max_x, 100000);
	#elif STORAGE == 0
		glb_gm.idx->NUMAStatus();
	#endif
	glb_gm.printGM();


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
	
	erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);
	glb_tpool.init_worker_threads();
	glb_tpool.init_syssweeper_threads();
	glb_tpool.init_router_threads(ds, wl, min_x, max_x, min_y, max_y, init_keys, values, machine_name);
	glb_tpool.init_ncoresweeper_threads();
	
	
	std::this_thread::sleep_for(std::chrono::milliseconds(300000));  // 200000(ycsb-a), 490000 (ini) 
	glb_tpool.terminate_ncoresweeper_threads();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	glb_tpool.dump_ncoresweeper_threads();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	exit(0);
	while(1);
}



