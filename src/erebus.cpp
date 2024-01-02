#include <iostream>
#include "storage/rtree/rtree.cpp"
#include <fstream>
using std::ifstream;
using std::ofstream;
using namespace erebus;
using namespace erebus::storage::rtree;
using namespace std;
#include <thread>   // std::thread
#include <mutex>

// #include <taskflow/taskflow.hpp>  // Taskflow is header-only
// #include "threads/BS_thread_pool.hpp"
#include "threads/threadpool.cc"
namespace erebus{

void build_basertree(int insert_strategy, int split_strategy) {
	RTree* tree = ConstructTree(50, 20);
	SetDefaultInsertStrategy(tree, insert_strategy);
	SetDefaultSplitStrategy(tree, split_strategy);
	int total_access = 0;
	ifstream ifs("./dataset/skew100k.txt", std::ifstream::in);
	for (int i = 0; i < 100000; i++) {
		double l, r, b, t;
		ifs >> l >> r >> b >> t;
		Rectangle* rectangle = InsertRec(tree, l, r, b, t);
		DefaultInsert(tree, rectangle);
	}
	ifs.close();

	ifs.open("./dataset/query1k.txt", std::ifstream::in);
	ofstream ofs("./reference.log", std::ofstream::out);
	for (int i = 0; i < 1000; i++) {
		//cout<<"query "<<i<<endl;
		double l, r, b, t;
		ifs >> l >> r >> b >> t;
		Rectangle query(l, r, b, t);
		int access = QueryRectangle(tree, l, r, b, t);
		ofs << tree->result_count << endl;
		total_access += access;
	}
	ofs.close();
	ifs.close();
	Clear(tree);
	cout << "insert strategy " << tree->insert_strategy_ << " split strategy " << tree->split_strategy_ << endl;
	cout << "average node access is " << 1.0 * total_access / 1000 << endl;
}

}   // namespace erebus


int main(){
	erebus::tp::ThreadPool gtp;
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