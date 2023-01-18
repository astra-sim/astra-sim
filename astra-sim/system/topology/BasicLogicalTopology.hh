/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __BASIC_LOGICAL_TOPOLOGY_HH__
#define __BASIC_LOGICAL_TOPOLOGY_HH__

#include "astra-sim/system/topology/LogicalTopology.hh"

namespace AstraSim {

class BasicLogicalTopology : public LogicalTopology {
 public:
  enum class BasicTopology { Ring = 0, BinaryTree };

  BasicLogicalTopology(BasicTopology basic_topology) {
    this->basic_topology = basic_topology;
  }

  virtual ~BasicLogicalTopology() = default;

  int get_num_of_dimensions() override {
    return 1;
  };

  virtual int get_num_of_nodes_in_dimension(int dimension) override = 0;

  BasicLogicalTopology* get_basic_topology_at_dimension(
      int dimension, ComType type) override {
    return this;
  }

  BasicTopology basic_topology;
};

} // namespace AstraSim

#endif /* __BASIC_LOGICAL_TOPOLOGY_HH__ */
