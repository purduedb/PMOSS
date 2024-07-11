#include "qtree.h"

namespace erebus
{
namespace storage
{
namespace qtree
{
//** Rect **//
Rect::Rect(const Rect &other) : Rect(other.x, other.y, other.width, other.height) { }
Rect::Rect(double _x, double _y, double _width, double _height) :
    x(_x),
    y(_y),
    width(_width),
    height(_height) {
}
bool Rect::contains(const Rect &other) const noexcept {
    if (x > other.x) return false;
    if (y > other.y) return false;
    if (x + width  < other.x + other.width) return false;
    if (y + height < other.y + other.height) return false;
    return true; // within bounds
}
bool Rect::intersects(const Rect &other) const noexcept {
    if (x > other.x + other.width)  return false;
    if (x + width < other.x)        return false;
    if (y > other.y + other.height) return false;
    if (y + height < other.y)       return false;
    return true; // intersection
}
double Rect::getLeft()   const noexcept { return x - (width  * 0.5f); }
double Rect::getTop()    const noexcept { return y + (height * 0.5f); }
double Rect::getRight()  const noexcept { return x + (width  * 0.5f); }
double Rect::getBottom() const noexcept { return y - (height * 0.5f); }

//** Collidable **//
Collidable::Collidable(const Rect &_bounds, std::any _data) :
    bound(_bounds),
    data(_data) {
};

//** QuadTree **//
QuadTree::QuadTree() : QuadTree({}, 0, 0) { }
QuadTree::QuadTree(const QuadTree &other) : QuadTree(other.bounds, other.capacity, other.maxLevel) { }
QuadTree::QuadTree(const Rect &_bound, unsigned _capacity, unsigned _maxLevel) :
    bounds(_bound),
    capacity(_capacity),
    maxLevel(_maxLevel) {
    objects.reserve(_capacity);
    // foundObjects.reserve(_capacity);
}

// Inserts an object into this quadtree
bool QuadTree::insert(Collidable *obj) {
    if (obj->qt != nullptr) 
        return false;

    if (!isLeaf) {
        // insert object into leaf
        if (QuadTree *child = getChild(obj->bound))
            return child->insert(obj);
    }
    objects.push_back(obj);
    obj->qt = this;

    // Subdivide if required
    if (isLeaf && level < maxLevel && objects.size() >= capacity) {
        subdivide();
        update(obj);
    }
    return true;
}

// Removes an object from this quadtree
bool QuadTree::remove(Collidable *obj) {
    if (obj->qt == nullptr) return false; // Cannot exist in vector
    if (obj->qt != this) return obj->qt->remove(obj);

    objects.erase(std::find(objects.begin(), objects.end(), obj));
    obj->qt = nullptr;
    discardEmptyBuckets();
    return true;
}

// Removes and re-inserts object into quadtree (for objects that move)
bool QuadTree::update(Collidable *obj) {
    if (!remove(obj)) return false;

    // Not contained in this node -- insert into parent
    if (parent != nullptr && !bounds.contains(obj->bound))
        return parent->insert(obj);
    if (!isLeaf) {
        // Still within current node -- insert into leaf
        if (QuadTree *child = getChild(obj->bound))
            return child->insert(obj);
    }
    return insert(obj);
}

// Searches quadtree for objects within the provided boundary and returns them in vector
int QuadTree::getObjectsInBound(const Rect &bound) {
    std::vector<Collidable*> result;
    
    list<QuadTree*> queue;
	queue.push_back(this);
	
    QuadTree* iter = this;
   
    while(!queue.empty()){
        iter = queue.front();
        // 	// -------------------------------------------------------------------------------------
		for (const auto &obj : iter->objects) {
            // Only check for intersection with OTHER boundaries
            if (&obj->bound != &bound && obj->bound.intersects(bound))
                result.push_back(obj);
        }
		// -------------------------------------------------------------------------------------
        queue.pop_front();
        if (iter->isLeaf) {}
		else {
			if (QuadTree *child = iter->getChild(bound)) {
                queue.push_back(child);
                
            } else {
                for (QuadTree *leaf : iter->children) {
                    if (leaf->bounds.intersects(bound)) {
                        queue.push_back(leaf);
                    }
                }
            }
		}

    }
    
    return result.size();
    // foundObjects.clear();
    // for (const auto &obj : objects) {
    //     // Only check for intersection with OTHER boundaries
    //     if (&obj->bound != &bound && obj->bound.intersects(bound))
    //         foundObjects.push_back(obj);
    // }
    // if (!isLeaf) {
    //     // Get objects from leaves
    //     if (QuadTree *child = getChild(bound)) {
    //         child->getObjectsInBound(bound);
    //         foundObjects.insert(foundObjects.end(), child->foundObjects.begin(), child->foundObjects.end());
    //     } else {
    //         for (QuadTree *leaf : children) {
    //             if (leaf->bounds.intersects(bound)) {
    //                 leaf->getObjectsInBound(bound);
    //                 // foundObjects.insert(foundObjects.end(), leaf->foundObjects.begin(), leaf->foundObjects.end());
    //             }
    //         }
    //     }
    // }
    // return foundObjects;
}

// Returns total children count for this quadtree
unsigned QuadTree::totalChildren() const noexcept {
    unsigned total = 0;
    if (isLeaf) return total;
    for (QuadTree *child : children)
        total += child->totalChildren();
    return 4 + total;
}

void QuadTree::NUMAStatus(NUMAstat &nstat){	
    void *ptr_to_check = this;
    int status[1];
    int ret_code = move_pages(0, 1, &ptr_to_check, NULL, status, 0);
    nstat.cntIndexNodes[status[0]] += 1;
    if (isLeaf) return;
	for (QuadTree *child : children)
        child->NUMAStatus(nstat);
	return;
}

// Returns total object count for this quadtree
unsigned QuadTree::totalObjects() const noexcept {
    unsigned total = (unsigned)objects.size();
    if (!isLeaf) {
        for (QuadTree *child : children)
            total += child->totalObjects();
    }
    return total;
}

// Removes all objects and children from this quadtree
void QuadTree::clear() noexcept {
    if (!objects.empty()) {
        for (auto&& obj : objects)
            obj->qt = nullptr;
        objects.clear();
    }
    if (!isLeaf) {
        for (QuadTree *child : children)
            child->clear();
        isLeaf = true;
    }
}

// Subdivides into four quadrants
void QuadTree::subdivide() {
    double width = bounds.width  * 0.5f;
    double height = bounds.height * 0.5f;
    double x = 0, y = 0;
    for (unsigned i = 0; i < 4; ++i) {
        switch (i) {
            case 0: x = bounds.x + width; y = bounds.y; break; // Top right
            case 1: x = bounds.x;         y = bounds.y; break; // Top left
            case 2: x = bounds.x;         y = bounds.y + height; break; // Bottom left
            case 3: x = bounds.x + width; y = bounds.y + height; break; // Bottom right
        }
        children[i] = new QuadTree({ x, y, width, height }, capacity, maxLevel);
        children[i]->level  = level + 1;
        children[i]->parent = this;
    }
    isLeaf = false;
}

// Discards buckets if all children are leaves and contain no objects
void QuadTree::discardEmptyBuckets() {
    if (!objects.empty()) return;
    if (!isLeaf) {
        for (QuadTree *child : children)
            if (!child->isLeaf || !child->objects.empty())
                return;
    }
    if (clear(), parent != nullptr)
        parent->discardEmptyBuckets();
}

// Returns child that contains the provided boundary
QuadTree *QuadTree::getChild(const Rect &bound) const noexcept {
    bool left  = bound.x + bound.width < bounds.getRight();
    bool right = bound.x > bounds.getRight();

    if (bound.y + bound.height < bounds.getTop()) {
        if (left)  return children[1]; // Top left
        if (right) return children[0]; // Top right
    } else if (bound.y > bounds.getTop()) {
        if (left)  return children[2]; // Bottom left
        if (right) return children[3]; // Bottom right
    }
    return nullptr; // Cannot contain boundary -- too large
}

QuadTree::~QuadTree() {
    clear();
    if (children[0]) delete children[0];
    if (children[1]) delete children[1];
    if (children[2]) delete children[2];
    if (children[3]) delete children[3];
}


void QuadTree::dfs(QuadTree* root){
    list<QuadTree*> queue_Visited;
    list<QuadTree*> queue_ToVisit;
	queue_ToVisit.push_back(root);
	QuadTree* iter = root;
    while(!queue_ToVisit.empty()){
        iter = queue_ToVisit.front();
        queue_Visited.push_back(iter);

        if (iter->isLeaf) {
            for (int i = 0; i < 4; i++) {
				QuadTree* node = iter->children[i];
				if (node != nullptr) {
                    std::cout << "something wrong here!";
                }
			}
        }
		else {
			for (int i = 0; i < 4; i++) {
				QuadTree* node = iter->children[i];
					queue_ToVisit.push_back(node);
			}
		}
        queue_ToVisit.pop_front();
    }
    std::cout << "size= " << queue_Visited.size() << std::endl;
    return;
}

int MigrateNodesQuery(QuadTree* root, const Rect &bound, int destNUMAID){
	list<QuadTree*> queue;
	queue.push_back(root);
	QuadTree* iter = root;
    if (!iter->bounds.intersects(bound)){
        return 0;
    }
    while(!queue.empty()){
        iter = queue.front();
        // 	// -------------------------------------------------------------------------------------
		// Move the node to a destination socket
		void *ptr_to_check = iter;
		int status[1];
		const int destNodes[1] = {destNUMAID};
		int ret_code = move_pages(0, 1, &ptr_to_check, destNodes, status, 0);
		// printf("Memory at %p is at %d node (retcode %d)\n", ptr_to_check, status[0], ret_code);
		// -------------------------------------------------------------------------------------
        queue.pop_front();
        if (iter->isLeaf) {}
		else {
			for (int i = 0; i < 4; i++) {
				QuadTree* node = iter->children[i];
				if (bound.intersects(node->bounds)) {
					queue.push_back(node);
				}
			}
		}

    }
    return 1;
    // if the node intersects the bound, then move its page 
    // void *ptr_to_check = root;
	// int status[1];
	// const int destNodes[1] = {destNUMAID};
	// int	ret_code = move_pages(0, 1, &ptr_to_check, destNodes, status, 0);
    // // printf("Memory at %p is at %d node (retcode %d)\n", ptr_to_check, status[0], ret_code);
    // if (!root->isLeaf) {
    //     // Get objects from leaves
    //     // if (QuadTree *child = getChild(bound)) {
    //     //     child->MigrateNodesQuery(bound, destNUMAID);
    //     // } else{ 
    //     for (QuadTree *leaf : root->children) {
    //         if (leaf->bounds.intersects(bound)) {
    //             MigrateNodesQuery(leaf, bound, destNUMAID);
    //         }
    //     }
    //     // }
    // }
    // return ret_code;
}
int MigrateNodesQuad(QuadTree* qtree, double left, double right, double bottom, double top, int destNUMAID) {
	Rect rec(left, bottom, (right-left), (top-bottom));
	MigrateNodesQuery(qtree, rec, destNUMAID);
	return -1;
}


}
}
}