/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "LocalRingGlobalBinaryTree.hh"
namespace AstraSim {
LocalRingGlobalBinaryTree::LocalRingGlobalBinaryTree(
    int id,
    BinaryTree::TreeType tree_type,
    int total_tree_nodes,
    int start,
    int stride,
    int local_dim) {
  this->local_dimension = new RingTopology(
      RingTopology::Dimension::Local, id, local_dim, id % local_dim, 1);
  this->global_dimension =
      new BinaryTree(id, tree_type, total_tree_nodes, start, stride);
}
LocalRingGlobalBinaryTree::~LocalRingGlobalBinaryTree() {
  delete local_dimension;
  delete global_dimension;
}
} // namespace AstraSim