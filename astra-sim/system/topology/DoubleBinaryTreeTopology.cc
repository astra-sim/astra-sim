/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/topology/DoubleBinaryTreeTopology.hh"

using namespace AstraSim;

DoubleBinaryTreeTopology::DoubleBinaryTreeTopology(
    int id, int total_tree_nodes, int start, int stride) {
  DBMAX = new BinaryTree(
      id, BinaryTree::TreeType::RootMax, total_tree_nodes, start, stride);
  DBMIN = new BinaryTree(
      id, BinaryTree::TreeType::RootMin, total_tree_nodes, start, stride);
  this->counter = 0;
}

DoubleBinaryTreeTopology::~DoubleBinaryTreeTopology() {
  delete DBMIN;
  delete DBMAX;
}

LogicalTopology* DoubleBinaryTreeTopology::get_topology() {
  BinaryTree* ans = nullptr;
  if (counter % 2 == 0) {
    ans = DBMAX;
  } else {
    ans = DBMIN;
  }
  counter++;

  return ans;
}

BasicLogicalTopology* DoubleBinaryTreeTopology::get_basic_topology_at_dimension(
    int dimension,
    ComType type) {
  if (dimension == 0) {
    return ((BinaryTree*)get_topology())
        ->get_basic_topology_at_dimension(0, type);
  } else {
    return nullptr;
  }
}

int DoubleBinaryTreeTopology::get_num_of_dimensions() {
  return 1;
}

int DoubleBinaryTreeTopology::get_num_of_nodes_in_dimension(int dimension) {
  return DBMIN->get_num_of_nodes_in_dimension(0);
}
