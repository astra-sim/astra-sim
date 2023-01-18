/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __LOGICAL_TOPOLOGY_HH__
#define __LOGICAL_TOPOLOGY_HH__

#include "astra-sim/system/Common.hh"

namespace AstraSim {

class BasicLogicalTopology;
class LogicalTopology {
 public:
  virtual ~LogicalTopology() = default;

  virtual LogicalTopology* get_topology() {
    return this;
  }
  virtual int get_num_of_dimensions() = 0;
  virtual int get_num_of_nodes_in_dimension(int dimension) = 0;
  virtual BasicLogicalTopology* get_basic_topology_at_dimension(
      int dimension, ComType type) = 0;
};

} // namespace AstraSim

#endif /* __LOGICAL_TOPOLOGY_HH__ */
