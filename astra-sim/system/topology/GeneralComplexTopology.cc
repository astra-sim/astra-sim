/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/topology/GeneralComplexTopology.hh"

#include <cassert>
#include <iostream>

#include "astra-sim/common/Logging.hh"
#include "astra-sim/system/topology/DoubleBinaryTreeTopology.hh"
#include "astra-sim/system/topology/RingTopology.hh"

using namespace std;
using namespace AstraSim;

GeneralComplexTopology::GeneralComplexTopology(
    int id,
    std::vector<int> dimension_size,
    std::vector<CollectiveImpl*> collective_impl) {
  int offset = 1;
  uint64_t last_dim = collective_impl.size() - 1;
  assert(collective_impl.size() <= dimension_size.size());
  for (uint64_t dim = 0; dim < collective_impl.size(); dim++) {
    if (collective_impl[dim]->type == CollectiveImplType::Ring ||
        collective_impl[dim]->type == CollectiveImplType::Direct ||
        collective_impl[dim]->type == CollectiveImplType::HalvingDoubling) {
      RingTopology* ring = new RingTopology(
          RingTopology::Dimension::NA,
          id,
          dimension_size[dim],
          (id % (offset * dimension_size[dim])) / offset,
          offset);
      dimension_topology.push_back(ring);
    } else if (
        collective_impl[dim]->type == CollectiveImplType::OneRing ||
        collective_impl[dim]->type == CollectiveImplType::OneDirect ||
        collective_impl[dim]->type == CollectiveImplType::OneHalvingDoubling) {
      int total_npus = 1;
      for (int d : dimension_size) {
        total_npus *= d;
      }
      RingTopology* ring = new RingTopology(
          RingTopology::Dimension::NA, id, total_npus, id % total_npus, 1);
      dimension_topology.push_back(ring);
      return;
    } else if (
        collective_impl[dim]->type == CollectiveImplType::DoubleBinaryTree) {
      if (dim == last_dim) {
        DoubleBinaryTreeTopology* DBT = new DoubleBinaryTreeTopology(
            id, dimension_size[dim], id % offset, offset);
        dimension_topology.push_back(DBT);
      } else {
        DoubleBinaryTreeTopology* DBT = new DoubleBinaryTreeTopology(
            id,
            dimension_size[dim],
            (id - (id % (offset * dimension_size[dim]))) + (id % offset),
            offset);
        dimension_topology.push_back(DBT);
      }
    }
    offset *= dimension_size[dim];
  }
}

GeneralComplexTopology::~GeneralComplexTopology() {
  for (uint64_t i = 0; i < dimension_topology.size(); i++) {
    delete dimension_topology[i];
  }
}

int GeneralComplexTopology::get_num_of_dimensions() {
  return dimension_topology.size();
}

int GeneralComplexTopology::get_num_of_nodes_in_dimension(int dimension) {
  if (static_cast<uint64_t>(dimension) >= dimension_topology.size()) {
    LoggerFactory::get_logger("system::topology::GeneralComplexTopology")
        ->critical(
            "dim: {} requested! but max dim is {}",
            dimension,
            dimension_topology.size() - 1);
  }
  assert(static_cast<uint64_t>(dimension) < dimension_topology.size());
  return dimension_topology[dimension]->get_num_of_nodes_in_dimension(0);
}

BasicLogicalTopology* GeneralComplexTopology::get_basic_topology_at_dimension(
    int dimension,
    ComType type) {
  return dimension_topology[dimension]->get_basic_topology_at_dimension(
      0, type);
}
