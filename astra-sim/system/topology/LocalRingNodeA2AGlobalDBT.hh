/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __LOCAL_RING_NODE_A2A_GLOBAL_DBT_HH__
#define __LOCAL_RING_NODE_A2A_GLOBAL_DBT_HH__

#include "astra-sim/system/topology/ComplexLogicalTopology.hh"
#include "astra-sim/system/topology/DoubleBinaryTreeTopology.hh"
#include "astra-sim/system/topology/RingTopology.hh"

namespace AstraSim {

class LocalRingNodeA2AGlobalDBT : public ComplexLogicalTopology {
 public:
  LocalRingNodeA2AGlobalDBT(
      int id,
      int local_dim,
      int node_dim,
      int total_tree_nodes,
      int start,
      int stride);
  ~LocalRingNodeA2AGlobalDBT();
  int get_num_of_nodes_in_dimension(int dimension) override;
  BasicLogicalTopology* get_basic_topology_at_dimension(
      int dimension,
      ComType type) override;
  int get_num_of_dimensions() override;

  DoubleBinaryTreeTopology* global_dimension_all_reduce;
  RingTopology* global_dimension_other;
  RingTopology* local_dimension;
  RingTopology* node_dimension;
};

} // namespace AstraSim

#endif /* __LOCAL_RING_NODE_A2A_GLOBAL_DBT_HH__ */
