/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "LocalRingNodeA2AGlobalDBT.hh"
namespace AstraSim {
LocalRingNodeA2AGlobalDBT::LocalRingNodeA2AGlobalDBT(
    int id,
    int total_tree_nodes,
    int start,
    int stride,
    int local_dim) {
  this->global_all_reduce_dimension = new DoubleBinaryTreeTopology(
      id, total_tree_nodes, start, stride, local_dim);
}
LocalRingNodeA2AGlobalDBT::~LocalRingNodeA2AGlobalDBT() {
  delete global_all_reduce_dimension;
}
} // namespace AstraSim