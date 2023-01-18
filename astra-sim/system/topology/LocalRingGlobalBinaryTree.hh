/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __LOCAL_RING_GLOBAL_BINARY_TREE_HH__
#define __LOCAL_RING_GLOBAL_BINARY_TREE_HH__

#include "astra-sim/system/topology/BinaryTree.hh"
#include "astra-sim/system/topology/ComplexLogicalTopology.hh"
#include "astra-sim/system/topology/RingTopology.hh"

namespace AstraSim {

class LocalRingGlobalBinaryTree : public ComplexLogicalTopology {
 public:
  LocalRingGlobalBinaryTree(
      int id,
      int local_dim,
      BinaryTree::TreeType tree_type,
      int total_tree_nodes,
      int start,
      int stride);
  ~LocalRingGlobalBinaryTree();
  int get_num_of_nodes_in_dimension(int dimension) override;
  int get_num_of_dimensions() override;
  BasicLogicalTopology* get_basic_topology_at_dimension(
      int dimension,
      ComType type) override;

  RingTopology* local_dimension;
  RingTopology* global_dimension_other;
  BinaryTree* global_dimension_all_reduce;
};

} // namespace AstraSim

#endif /* __LOCAL_RING_GLOBAL_BINARY_TREE_HH__ */
