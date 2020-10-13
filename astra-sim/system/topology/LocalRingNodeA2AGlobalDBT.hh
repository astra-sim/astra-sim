/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __LOCALRINGNODEA2AGLOBALDBT_HH__
#define __LOCALRINGNODEA2AGLOBALDBT_HH__

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
#include "DoubleBinaryTreeTopology.hh"
#include "astra-sim/system/Common.hh"

namespace AstraSim {
class LocalRingNodeA2AGlobalDBT : public ComplexLogicalTopology {
 public:
  DoubleBinaryTreeTopology* global_all_reduce_dimension;
  LocalRingNodeA2AGlobalDBT(
      int id,
      int total_tree_nodes,
      int start,
      int stride,
      int local_dim);
  ~LocalRingNodeA2AGlobalDBT();
};
} // namespace AstraSim
#endif