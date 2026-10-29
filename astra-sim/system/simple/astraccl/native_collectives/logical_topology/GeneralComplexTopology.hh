// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <vector>

#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/ComplexLogicalTopology.hh>

namespace AstraSim {

class GeneralComplexTopology : public ComplexLogicalTopology {
  public:
    GeneralComplexTopology(int id,
                           std::vector<int> dimension_size,
                           std::vector<CollectiveImpl*> collective_impl);
    ~GeneralComplexTopology();

    int get_num_of_dimensions() override;
    int get_num_of_nodes_in_dimension(int dimension) override;
    BasicLogicalTopology* get_basic_topology_at_dimension(
        int dimension, ComType type) override;

    std::vector<LogicalTopology*> dimension_topology;
};

}  // namespace AstraSim
