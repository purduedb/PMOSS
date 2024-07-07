/**
 * @file qtree.cpp
 * @author https://github.com/psimatis/QuadTree/tree/main
 * @brief 
 */

// #ifndef QUADTREE_QUADTREE_H
// #define QUADTREE_QUADTREE_H

// -------------------------------------------------------------------------------------
#include <bits/stdc++.h>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
// Comment accordingly to select your preferred split strategy
// #define POINT_SPLIT true     // Optimized Point-Quad-Tree: splits the bucket on the median point
#define POINT_SPLIT false    // Point-Region-Quad-Tree: divides the bucket to equal sized regions

#define CAPACITY 128

#define XLOW 0
#define YLOW 1
#define XHIGH 2
#define YHIGH 3

#define NW 0
#define NE 1
#define SW 2
#define SE 3

namespace erebus
{
namespace storage
{
namespace qtree
{

class Record {
public:
    float id;
    char type;
    vector<float> box = vector<float>(4);

    Record();
    Record(float id, vector<float>); // for data
    Record(char type, vector<float> boundary, float info); // for query
	bool contains(Record);
    bool operator < (const Record& rhs) const;
    array<float, 2> toKNNPoint();
    ~Record();

    bool intersects(Record r);
};




class Input : public vector<Record>
{
public:
    Input();
    void loadData(const char *filename, int limit);
    void loadQueries(const char *filename);
    void sortData();
    ~Input();
};






class QuadTreeNode{
public :
    vector<QuadTreeNode*> children = vector<QuadTreeNode*>(4);
    Input data;
    int level;
    vector<float> box;
    
    QuadTreeNode(vector<float> boundary, int level);
    void insert(Record);
    bool intersects(Record r);
    void rangeQuery(Record q, vector<float> &results, map<string, double> &map);
    void kNNQuery(array<float, 2> p, map<string, double> &stats, int k);
    void deleteTree();
    void calculateSize(int &);
    void getTreeHeight(int &);
    void snapshot();
    double minSqrDist(array<float, 4> r) const;
    void packing(Input &R);
    void packing();
    void divide();
    void count(int &, int &, int &, int &);
    bool isLeaf();
    void getStatistics();
    ~QuadTreeNode();
};

// #endif //QUADTREE_QUADTREE_H

}  // namespace qtree
}  // namespace storage
}  // namespace erebus