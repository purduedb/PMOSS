#ifndef PMOSS_INDEX_H_
#define PMOSS_INDEX_H_
// -------------------------------------------------------------------------------------
#pragma once
#include <iostream>
// -------------------------------------------------------------------------------------
#include <byteswap.h>
// -------------------------------------------------------------------------------------
// #define BTREE_SLOWER_LAYOUT
#include "indexkey.h"
#ifndef BTREE_SLOWER_LAYOUT
#include "btree/btree.h"
#else
#include "btree/btree.h"
#endif
// -------------------------------------------------------------------------------------

namespace erebus
{
namespace storage
{

template<typename KeyType, class KeyComparator>
class Index
{
 public:
  // Used for migrating nodes to certain numa nodes
  virtual uint64_t migrate(KeyType key1, KeyType key2, int range, int destNUMA) = 0;
  
  // Prints the location of each index node page
  virtual uint64_t count_numa_division(KeyType key1, KeyType key2, int range) = 0;

  virtual bool insert(KeyType key, uint64_t value) = 0;

  virtual uint64_t find(KeyType key, std::vector<uint64_t> *v) = 0;

  virtual uint64_t find_bwtree_fast(KeyType key, std::vector<uint64_t> *v) {};

  // Used for bwtree only
  virtual bool insert_bwtree_fast(KeyType key, uint64_t value) {};

  virtual bool upsert(KeyType key, uint64_t value) = 0;

  virtual uint64_t scan(KeyType key, int range) = 0;

  virtual int64_t getMemory() const = 0;

  // This initializes the thread pool
  virtual void UpdateThreadLocal(size_t thread_num) = 0;
  virtual void AssignGCID(size_t thread_id) = 0;
  virtual void UnregisterThread(size_t thread_id) = 0;
  
  // After insert phase perform this action
  // By default it is empty
  // This will be called in the main thread
  virtual void AfterLoadCallback() {}
  
  // This is called after threads finish but before the thread local are
  // destroied by the thread manager
  virtual void CollectStatisticalCounter(int) {}
  virtual size_t GetIndexSize() { return 0UL; }

  // Destructor must also be virtual
  virtual ~Index() {}
};




template<typename KeyType, class KeyComparator>
class BTreeOLCIndex : public Index<KeyType, KeyComparator>
{
 public:

  ~BTreeOLCIndex() {
  }

  void UpdateThreadLocal(size_t thread_num) {}
  void AssignGCID(size_t thread_id) {}
  void UnregisterThread(size_t thread_id) {}

  bool insert(KeyType key, uint64_t value) {
    idx.insert(key, value);
    return true;
  }

  uint64_t find(KeyType key, std::vector<uint64_t> *v) {
    uint64_t result;
    idx.lookup(key,result);
    v->clear();
    v->push_back(result);
    return 0;
  }

  uint64_t count_numa_division(KeyType key1, KeyType key2, int range) {
    bool is_first = true;
    uint64_t results[range];  // contains keys
    uint64_t numa_nodes[16] = {0};  
    
    uint64_t count = idx.full_scan(key1, key2, results, numa_nodes, is_first);
    is_first = false;
    
    if (count==0)
       return 0;

    while (count < range) {
      KeyType nextKey = results[count-1];
      incKey(nextKey); // hack: this only works for fixed-size keys
      if (nextKey > key2)
        break;
      
      // uint64_t nextCount = idx.migratoryScan(nextKey, key2, range - count, results + count, destNUMA);
      // We do not need the keys here at all, so we can override the keys of the previous result
      uint64_t nextCount = idx.full_scan(nextKey, key2, results, numa_nodes, is_first);
      
      if (nextCount==0)
        break; // no more entries
      count = nextCount;
    }

    for(int i=0; i<8; i++)
      cout << numa_nodes[i] << ' ';
    cout << endl;
    // for(int i=0; i<100; i++)
    //   cout << results[i] << endl;
    return 1;

  }
  
  bool upsert(KeyType key, uint64_t value) {
    idx.insert(key, value);
    return true;
  }

  void incKey(uint64_t& key) { key++; };
  void incKey(GenericKey<31>& key) { key.data[strlen(key.data)-1]++; };

  uint64_t scan(KeyType key, int range) {
    uint64_t results[range];
    uint64_t count = idx.scan(key, range, results);
    if (count==0)
       return 0;

    while (count < range) {
      KeyType nextKey = *reinterpret_cast<KeyType*>(results[count-1]);
      // ISSUE: issue a range scan for the 3rd largest key and range_size = 100
      //  what will happen is count = 3, enter the while loop, nextkey will get you to an invalid value
      //  that does not exist
      //  One solution: the range size have to be less than what you can get
      incKey(nextKey); // hack: this only works for fixed-size keys
      
      uint64_t nextCount = idx.scan(nextKey, range - count, results + count);
      if (nextCount==0)
        break; // no more entries
      count += nextCount;
    }
    return count;
  }

  uint64_t migrate(KeyType key1, KeyType key2, int range, int destNUMA) {
    uint64_t results[range];
    uint64_t count = idx.migratory_scan(key1, key2, range, results, destNUMA);
    if (count==0)
       return 0;

    while (count < range) {
      KeyType nextKey = *reinterpret_cast<KeyType*>(results[count-1]);
      incKey(nextKey); // hack: this only works for fixed-size keys
      if (nextKey > key2)
        break;
      
      uint64_t nextCount = idx.migratory_scan(nextKey, key2, range - count, results+count, destNUMA);
      if (nextCount==0)
        break; // no more entries
      count += nextCount;
    }
    return count;

    // uint64_t leaf_count = 1;
    // uint64_t results[range];
    // uint64_t count = idx.migratoryScan(key1, key2, range, results, destNUMA);

    // if (count==0)
    //    return 0;

    // while (count < range) {
    //   KeyType nextKey = results[count-1];
    //   incKey(nextKey); // hack: this only works for fixed-size keys
    //   if (nextKey > key2)
    //     break;
      
    //   // uint64_t nextCount = idx.migratoryScan(nextKey, key2, range - count, results + count, destNUMA);
    //   // We do not need the keys here at all, so we can override the keys of the previous result
    //   uint64_t nextCount = idx.migratoryScan(nextKey, key2, range, results, destNUMA);
      
    //   if (nextCount==0)
    //     break; // no more entries
    //   count = nextCount;
    //   leaf_count += 1;
    // }
    // return leaf_count;
  }


  int64_t getMemory() const {
    return 0;
  }

  void merge() {}

  BTreeOLCIndex(uint64_t kt) {}

 private:
  btree::BTree<KeyType,uint64_t> idx;
};


}
}


#endif 