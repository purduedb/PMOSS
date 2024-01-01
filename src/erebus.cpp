#include <iostream>
#include "storage/rtree/rtree.cpp"
#include <fstream>
using std::ifstream;
using std::ofstream;
using namespace erebus;
using namespace erebus::storage::rtree;
#include <thread>   // std::thread
#include <mutex>

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
    build_basertree(1, 1);

    unsigned num_cpus = std::thread::hardware_concurrency();
    std::cout << "Launching " << num_cpus << " threads\n";
    
    constexpr unsigned num_threads = 20;
    std::vector<std::thread> threads(num_threads);
    std::mutex iomutex;
    


    return 0;
}