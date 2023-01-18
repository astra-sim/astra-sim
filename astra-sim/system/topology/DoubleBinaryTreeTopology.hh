/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __DOUBLE_BINARY_TREE_TOPOLOGY_HH__
#define __DOUBLE_BINARY_TREE_TOPOLOGY_HH__

#include "astra-sim/system/topology/ComplexLogicalTopology.hh"
#include "astra-sim/system/topology/LocalRingGlobalBinaryTree.hh"

namespace AstraSim {

class DoubleBinaryTreeTopology : public ComplexLogicalTopology {
 public:
  DoubleBinaryTreeTopology(int id, int total_tree_nodes, int start, int stride);
  ~DoubleBinaryTreeTopology();
  LogicalTopology* get_topology() override;
  BasicLogicalTopology* get_basic_topology_at_dimension(
      int dimension, ComType type) override;
  int get_num_of_dimensions() override;
  int get_num_of_nodes_in_dimension(int dimension) override;

  int counter;
  BinaryTree* DBMAX;
  BinaryTree* DBMIN;
};

} // namespace AstraSim

#endif /* __DOUBLE_BINARY_TREE_TOPOLOGY_HH__ */
