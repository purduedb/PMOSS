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
  virtual uint64_t migrate(KeyType key, int range, int destNUMA) = 0;

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
      incKey(nextKey); // hack: this only works for fixed-size keys

      uint64_t nextCount = idx.scan(nextKey, range - count, results + count);
      if (nextCount==0)
        break; // no more entries
      count += nextCount;
    }
    return count;
  }

  uint64_t migrate(KeyType key, int range, int destNUMA) {
    uint64_t results[range];
    uint64_t count = idx.migratoryScan(key, range, results, destNUMA);
    if (count==0)
       return 0;

    while (count < range) {
      KeyType nextKey = *reinterpret_cast<KeyType*>(results[count-1]);
      incKey(nextKey); // hack: this only works for fixed-size keys

      uint64_t nextCount = idx.migratoryScan(nextKey, range - count, results + count, destNUMA);
      if (nextCount==0)
        break; // no more entries
      count += nextCount;
    }
    return count;
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



