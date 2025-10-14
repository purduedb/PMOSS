#include "erebus.hpp"
// -------------------------------------------------------------------------------------
#include <iostream>
#include <fstream>
#include <thread>   // std::thread
#include <mutex>
#include <chrono>
#include <vector>
// -------------------------------------------------------------------------------------
using std::ifstream;
using std::ofstream;
// -------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{	
	auto experiment_start = std::chrono::high_resolution_clock::now();
	int cfgIdx = 1;
	int ds = YCSB;
	int iam = BTREE;
	
	if (argc > 1) {
		cfgIdx = std::atoi(argv[1]);
	}
	
	cout << "=== P-MOSS Workload Drift Experiment ===" << endl;
	cout << "CONFIG=" << cfgIdx << endl;
	
	// Keys in database 
	std::vector<keytype> init_keys;
	init_keys.reserve(SINGLE_DIMENSION_KEY_LIMIT);
	
	// Pointers to the keys
	std::vector<uint64_t> values;
	values.reserve(SINGLE_DIMENSION_KEY_LIMIT);
	
	double min_x, max_x, min_y, max_y;
	if (ds == OSM_USNE){
		min_x = -180;	max_x = 180;
		min_y = -90;	max_y = 90;
	}
	else if (ds == GEOLITE){
		min_x = -180;	max_x = 180;
		min_y = -90;	max_y = 90;
	}
	else if (ds == BERLINMOD02){
		min_x = 0;		max_x = 40000;
		min_y = 0;		max_y = 40000;
	}
	else if (ds == YCSB){
		min_x = 0;		max_x = 1000000000;
		min_y = 0;		max_y = 1000000000;
	}

	// Hardware topology discovery
	erebus::utils::HWTopologyInit();
	vector<CPUID> cpuIds = erebus::utils::getCPUIDs();
	vector<NUMAID> numaIds = erebus::utils::getNUMAIDs();
	int num_cores = erebus::utils::getNCores();
	int num_NUMA_nodes = erebus::utils::getNNUMANodes();
	
	cout << "Hardware Configuration:" << endl;
	cout << "  Cores: " << num_cores << ", NUMA Nodes: " << num_NUMA_nodes << endl;
	
	// CPU pool initialization
	vector<vector<CPUID>> cPool;
	cPool.resize(num_NUMA_nodes);
	erebus::utils::initCorePool(cPool, cpuIds, numaIds);
	erebus::utils::printCorePool(cPool, num_NUMA_nodes);

	// Grid Manager initialization
	erebus::dm::GridManager glb_gm(cfgIdx, SD_YCSB_WKLOADA, iam, 16, 16, min_x, max_x, min_y, max_y);
	
	// Initialize CPU vectors for different thread types
	std::vector<CPUID> ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids; 
	
	int num_workers = 0;
	#if MACHINE == 0
		num_workers = 10;
		ss_cpuids.push_back(0);
		mm_cpuids.push_back(12);
	#elif MACHINE == 6
		num_workers = 10;  
		ss_cpuids.push_back(0);
		mm_cpuids.push_back(24);
	#else
		num_workers = 7;  
	#endif
	
	// Configure thread assignment based on machine type
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
	
	// Build the index based on storage type
	#if STORAGE == 0
		db.build_rtree(ds, 1, 1);
		glb_gm.register_index(db.idx);
	#elif STORAGE == 1
		db.build_idx(ds, min_x, max_x, min_y, max_y);
		glb_gm.register_index(db.idx_quadtree);
	#elif STORAGE == 2
		int kt = RAND_KEY;
		db.build_btree(ds, kt, init_keys, values);
		glb_gm.register_index(db.idx_btree);
	#endif

	// Configuration file selection
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
	#elif MACHINE==6
		#if MAX_GRID_CELL == 100
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_4n/" + std::to_string(SD_YCSB_WKLOADA)
			+ "/c_" + std::to_string(cfgIdx) + ".txt";
		#else 
		config_file = std::string(PROJECT_SOURCE_DIR) + "/src/pmoss_machine_configs/intel_skx_4s_4n/" + std::to_string(SD_YCSB_WKLOADA)
			+ "/c_" + std::to_string(cfgIdx)+ "_" + std::to_string(MAX_GRID_CELL) + ".txt";
		#endif 
	#endif
	#endif
	}
	
	cout << "Using config file: " << config_file << endl;
	glb_gm.register_grid_cells(config_file);
	glb_gm.buildDataDistIdx(iam, init_keys);
	glb_gm.enforce_scheduling();

	// Initialize thread pool
	erebus::tp::TPManager glb_tpool(ncore_cpuids, ss_cpuids, mm_cpuids, wrk_cpuids, rt_cpuids, &glb_gm, &glb_rm);

	glb_tpool.init_worker_threads();
	glb_tpool.init_syssweeper_threads();
	glb_tpool.init_megamind_threads();
	glb_tpool.init_ncoresweeper_threads();

	// === WORKLOAD DRIFT EXPERIMENT BEGINS ===
	std::vector<int> drift_workloads = {SD_YCSB_WKLOADA, SD_YCSB_WKLOADC, SD_YCSB_WKLOADE};
	std::vector<std::string> drift_names = {"WKLOADA", "WKLOADC", "WKLOADE"};
	const int drift_duration_ms = 600000; // 10 minutes per workload (600,000 ms)
	
	cout << "\n=== Starting Workload Drift Experiment ===" << endl;
	cout << "Each workload will run for " << drift_duration_ms/1000/60 << " minutes" << endl;
	cout << "Total experiment time: " << (drift_workloads.size() * drift_duration_ms)/1000/60 << " minutes" << endl;
	
	auto drift_start = std::chrono::high_resolution_clock::now();
	
	// Performance tracking structures
	struct WorkloadPhaseStats {
		std::string workload_name;
		int workload_id;
		std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
		std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
		double duration_seconds;
		// Add more performance metrics as needed
	};
	
	std::vector<WorkloadPhaseStats> phase_stats;
	
	for (size_t phase = 0; phase < drift_workloads.size(); phase++) {
		int current_wl = drift_workloads[phase];
		WorkloadPhaseStats stats;
		stats.workload_name = drift_names[phase];
		stats.workload_id = current_wl;
		stats.start_time = std::chrono::high_resolution_clock::now();
		
		cout << "\n=== Phase " << (phase + 1) << "/3: Starting " << drift_names[phase] 
		     << " (workload=" << current_wl << ") ===" << endl;
		
		// Terminate existing router threads if not first phase
		if (phase > 0) {
			cout << "Terminating previous workload threads..." << endl;
			glb_tpool.terminate_router_threads();
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
		
		// Initialize router threads with new workload
		cout << "Initializing " << drift_names[phase] << " workload threads..." << endl;
		glb_tpool.init_router_threads(ds, current_wl, min_x, max_x, min_y, max_y, init_keys, values);
		
		cout << "Running " << drift_names[phase] << " for " << drift_duration_ms/1000 << " seconds..." << endl;
		
		// Run current workload phase with periodic status updates
		const int status_interval_ms = 60000; // 1 minute status updates
		int elapsed_time = 0;
		while (elapsed_time < drift_duration_ms) {
			int sleep_time = std::min(status_interval_ms, drift_duration_ms - elapsed_time);
			std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
			elapsed_time += sleep_time;
			
			if (elapsed_time < drift_duration_ms) {
				cout << "  " << drift_names[phase] << " - " << elapsed_time/1000/60 
				     << "/" << drift_duration_ms/1000/60 << " minutes completed" << endl;
			}
		}
		
		stats.end_time = std::chrono::high_resolution_clock::now();
		stats.duration_seconds = std::chrono::duration<double>(stats.end_time - stats.start_time).count();
		phase_stats.push_back(stats);
		
		cout << "=== Phase " << (phase + 1) << " (" << drift_names[phase] << ") completed in " 
		     << stats.duration_seconds << " seconds ===" << endl;
		
		// Print grid performance statistics for this phase
		cout << "--- Performance Statistics for " << drift_names[phase] << " ---" << endl;
		glb_gm.printQueryDistPushed();
		glb_gm.printQueryDistCompleted();
		glb_gm.printQueryDistOstanding();
		cout << "--- End Performance Statistics ---" << endl;
	}
	
	// Final cleanup and summary
	auto drift_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> total_drift_elapsed = drift_end - drift_start;
	
	cout << "\n=== Workload Drift Experiment Completed ===" << endl;
	cout << "Total experiment duration: " << total_drift_elapsed.count() << " seconds" << endl;
	
	// Print detailed phase summary
	cout << "\n=== Phase Summary ===" << endl;
	for (const auto& stats : phase_stats) {
		cout << "Phase: " << stats.workload_name << " (ID: " << stats.workload_id << ")" << endl;
		cout << "  Duration: " << stats.duration_seconds << " seconds (" 
		     << stats.duration_seconds/60 << " minutes)" << endl;
		cout << "  Performance: [Performance metrics would be added here]" << endl;
	}
	
	glb_tpool.terminate_router_threads();
	glb_tpool.terminate_ncoresweeper_threads();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	glb_tpool.dump_ncoresweeper_threads();
	
	auto finish = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> total_elapsed = finish - experiment_start;
	cout << "\nTotal execution time: " << total_elapsed.count() << " seconds" << endl;
	
	// Generate summary report
	cout << "\n=== PMOSS Workload Drift Performance Report ===" << endl;
	cout << "Configuration: " << cfgIdx << endl;
	cout << "Storage Type: BTREE" << endl;
	cout << "Grid Size: " << MAX_GRID_CELL << " cells" << endl;
	cout << "Workload Sequence: A -> C -> E" << endl;
	cout << "Phase Duration: " << drift_duration_ms/1000/60 << " minutes each" << endl;
	cout << "Total Experiment Time: " << total_elapsed.count()/60 << " minutes" << endl;
	cout << "=== End Report ===" << endl;
	
	exit(0);
}