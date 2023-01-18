/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __COMPLEX_LOGICAL_TOPOLOGY_HH__
#define __COMPLEX_LOGICAL_TOPOLOGY_HH__

#include "astra-sim/system/topology/LogicalTopology.hh"

namespace AstraSim {

class ComplexLogicalTopology : public LogicalTopology {
 public:
  ComplexLogicalTopology() { }
  virtual ~ComplexLogicalTopology() = default;

  virtual int get_num_of_dimensions() override = 0;
  virtual int get_num_of_nodes_in_dimension(int dimension) override = 0;
  virtual BasicLogicalTopology* get_basic_topology_at_dimension(
      int dimension, ComType type) override = 0;
};

} // namespace AstraSim

#endif /* __COMPLEX_LOGICAL_TOPOLOGY_HH__ */
