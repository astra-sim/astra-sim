/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __LOCALRINGGLOBALBINARYTREE_HH__
#define __LOCALRINGGLOBALBINARYTREE_HH__

#include <assert.h>
#include <math.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <list>
#include <map>
#include <sstream>
#include <tuple>
#include <vector>
#include "BinaryTree.hh"
#include "ComplexLogicalTopology.hh"
#include "RingTopology.hh"
#include "astra-sim/system/Common.hh"

namespace AstraSim {
class LocalRingGlobalBinaryTree : public ComplexLogicalTopology {
 public:
  RingTopology* local_dimension;
  BinaryTree* global_dimension;
  LocalRingGlobalBinaryTree(
      int id,
      BinaryTree::TreeType tree_type,
      int total_tree_nodes,
      int start,
      int stride,
      int local_dim);
  ~LocalRingGlobalBinaryTree();
};
} // namespace AstraSim
#endif