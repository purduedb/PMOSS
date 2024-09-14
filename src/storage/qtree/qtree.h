#ifndef PMOSS_QUADTREE_H_
#define PMOSS_QUADTREE_H_

// -------------------------------------------------------------------------------------
#pragma once
#include <any>
#include <vector>
#include <algorithm>
#include<list>
#include<queue>
#include <iostream>
// -------------------------------------------------------------------------------------
#include <numa.h> 
#include <numaif.h>
#include <stdio.h>
// -------------------------------------------------------------------------------------
using std::list;

namespace erebus
{
namespace storage
{
namespace qtree
{

struct NUMAstat {
    int cntIndexNodes[8] = {0};
};

struct Rect {
    double x, y, width, height;

    bool contains(const Rect &other) const noexcept;
    bool intersects(const Rect &other) const noexcept;
    double getLeft() const noexcept;
    double getTop() const noexcept;
    double getRight() const noexcept;
    double getBottom() const noexcept;

    Rect(const Rect&);
    Rect(double _x = 0, double _y = 0, double _width = 0, double _height = 0);
};

class QuadTree;

struct Collidable {
    friend class QuadTree;
public:
    Rect bound;
    std::any data;

    Collidable(const Rect &_bounds = {}, std::any _data = {});
private:
    QuadTree *qt = nullptr;
    Collidable(const Collidable&) = delete;
};

class QuadTree {
public:
    QuadTree(const Rect &_bound, unsigned _capacity, unsigned _maxLevel);
    QuadTree(const QuadTree&);
    QuadTree();

    bool insert(Collidable *obj);
    bool remove(Collidable *obj);
    bool update(Collidable *obj);
    int getObjectsInBound(const Rect &bound);
    
    void NUMAStatus(NUMAstat &nstat);

    unsigned totalChildren() const noexcept;
    unsigned totalObjects() const noexcept;
    void clear() noexcept;

    ~QuadTree();
// private:
    bool      isLeaf = true;
    unsigned  level  = 0;
    unsigned  capacity;
    unsigned  maxLevel;
    Rect      bounds;
    QuadTree* parent = nullptr;
    QuadTree* children[4] = { nullptr, nullptr, nullptr, nullptr };
    std::vector<Collidable*> objects, foundObjects;

    void subdivide();
    void discardEmptyBuckets();
    inline QuadTree *getChild(const Rect &bound) const noexcept;
    void dfs(QuadTree* root);
};

extern "C"{
    int MigrateNodesQuad(QuadTree* qtree, double left, double right, double bottom, double top, int destNUMAID);
    int MigrateNodesQuery(QuadTree* qtree, const Rect &bound, int destNUMAID);
}

}
}
}

#endif 