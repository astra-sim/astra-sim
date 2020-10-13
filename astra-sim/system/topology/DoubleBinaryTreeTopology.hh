/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __DOUBLEBINARYTREETOPOLOGY_HH__
#define __DOUBLEBINARYTREETOPOLOGY_HH__

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
#include "ComplexLogicalTopology.hh"
#include "LocalRingGlobalBinaryTree.hh"
#include "astra-sim/system/Common.hh"
namespace AstraSim {
class DoubleBinaryTreeTopology : public ComplexLogicalTopology {
 public:
  int counter;
  LocalRingGlobalBinaryTree* DBMAX;
  LocalRingGlobalBinaryTree* DBMIN;
  LogicalTopology* get_topology();
  ~DoubleBinaryTreeTopology();
  DoubleBinaryTreeTopology(
      int id,
      int total_tree_nodes,
      int start,
      int stride,
      int local_dim);
};
} // namespace AstraSim
#endif