#include <iostream>
#include "utils/gflags.h"

#include <fstream>
using std::ifstream;
using std::ofstream;
#include <thread>   // std::thread
#include <mutex>
#include "threads/threadpool.hpp"

// #include <taskflow/taskflow.hpp>  // Taskflow is header-only
// #include "threads/BS_thread_pool.hpp"
using namespace erebus;
using namespace erebus::storage::rtree;
using namespace erebus::tp;
namespace erebus{

RTree* tree;
void build_basertree(int insert_strategy, int split_strategy) {
	tree = ConstructTree(50, 20);
	SetDefaultInsertStrategy(tree, insert_strategy);
	SetDefaultSplitStrategy(tree, split_strategy);
	int total_access = 0;
	ifstream ifs("/home/yrayhan/works/erebus/src/dataset/uni100k.txt", std::ifstream::in);
	for (int i = 0; i < 100000; i++) {
		double l, r, b, t;
		ifs >> l >> r >> b >> t;
		Rectangle* rectangle = InsertRec(tree, l, r, b, t);
		DefaultInsert(tree, rectangle);
	}
	ifs.close();

	// ifs.open("./dataset/query1k.txt", std::ifstream::in);
	// ofstream ofs("./reference.log", std::ofstream::out);
	// for (int i = 0; i < 1000; i++) {
	// 	//cout<<"query "<<i<<endl;
	// 	double l, r, b, t;
	// 	ifs >> l >> r >> b >> t;
	// 	Rectangle query(l, r, b, t);
	// 	int access = QueryRectangle(tree, l, r, b, t);
	// 	ofs << tree->result_count << endl;
	// 	total_access += access;
	// }
	// ofs.close();
	// ifs.close();
	// Clear(tree);
	cout << "insert strategy " << tree->insert_strategy_ << " split strategy " << tree->split_strategy_ << endl;
	cout << "average node access is " << 1.0 * total_access / 1000 << endl;
}

}   // namespace erebus


int main(){

	// T1: Build the index first
	build_basertree(1, 1);

	// T2: Start the threads 
	// Megamind threads are the ones that receive the queries and pass it to the worker threads, 
	// The worker threads actually do the job of querying 
	
	std::vector<int> mm_cpuids = {30, 31};
	std::vector<int> wrk_cpuids = {11, 12, 13, 14};
	std::vector<int> rt_cpuids = {99, 100};
	
	ThreadPool glb_tpool(mm_cpuids, wrk_cpuids, rt_cpuids, tree);
	
	
	while(1);
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

    return 0;
}