//
//  generator.h
//  YCSB-cpp
//
//  Created by Jinglei Ren on 12/6/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#ifndef YCSB_C_GENERATOR_H_
#define YCSB_C_GENERATOR_H_

#include <cstdint>
namespace erebus
{
namespace generators
{

template <typename Value>
class Generator {
 public:
  virtual Value Next() = 0;
  virtual Value Last() = 0;
  virtual ~Generator() { }
};


}  // generators
}  // erebus

#endif // YCSB_C_GENERATOR_H_
