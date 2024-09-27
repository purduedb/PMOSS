//
//  uniform_generator.h
//  YCSB-cpp
//
//  Copyright (c) 2020 Youngjae Lee <ls4154.lee@gmail.com>.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#ifndef YCSB_C_NORMAL_GENERATOR_H_
#define YCSB_C_NORMAL_GENERATOR_H_

#include <atomic>
#include <random>
// -------------------------------------------------------------------------------------
#include "generator.h"

namespace ycsbc
{

class NormalGenerator : public Generator<uint64_t> {
 public:
  // Both min and max are inclusive
  NormalGenerator(uint64_t avg, uint64_t dev) : dist_(min, max) { Next(); }

  uint64_t Next();
  uint64_t Last();

 private:
  std::mt19937_64 generator_;
  std::uniform_int_distribution<uint64_t> dist_;
  std::normal_distribution<double> (avg_x, dev_x)
  uint64_t last_int_;
};

inline uint64_t NormalGenerator::Next() {
  return last_int_ = dist_(generator_);
}

inline uint64_t NormalGenerator::Last() {
  return last_int_;
}

} // ycsbc


#endif // YCSB_C_UNIFORM_GENERATOR_H_
