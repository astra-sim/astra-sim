/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __GENERAL_COMPLEX_TOPOLOGY_HH__
#define __GENERAL_COMPLEX_TOPOLOGY_HH__

#include <vector>

#include "astra-sim/system/topology/ComplexLogicalTopology.hh"

namespace AstraSim {

class GeneralComplexTopology : public ComplexLogicalTopology {
 public:
  GeneralComplexTopology(
      int id,
      std::vector<int> dimension_size,
      std::vector<CollectiveImpl*> collective_impl);
  ~GeneralComplexTopology();

  int get_num_of_dimensions() override;
  int get_num_of_nodes_in_dimension(int dimension) override;
  BasicLogicalTopology* get_basic_topology_at_dimension(
      int dimension, ComType type) override;

  std::vector<LogicalTopology*> dimension_topology;
};

} // namespace AstraSim

#endif /* __GENERAL_COMPLEX_TOPOLOGY_HH__ */
