// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/ComplexLogicalTopology.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/DoubleBinaryTreeTopology.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/RingTopology.hh>

namespace AstraSim {

class LocalRingNodeA2AGlobalDBT : public ComplexLogicalTopology {
  public:
    LocalRingNodeA2AGlobalDBT(int id,
                              int local_dim,
                              int node_dim,
                              int total_tree_nodes,
                              int start,
                              int stride);
    ~LocalRingNodeA2AGlobalDBT();
    int get_num_of_nodes_in_dimension(int dimension) override;
    BasicLogicalTopology* get_basic_topology_at_dimension(
        int dimension, ComType type) override;
    int get_num_of_dimensions() override;

    DoubleBinaryTreeTopology* global_dimension_all_reduce;
    RingTopology* global_dimension_other;
    RingTopology* local_dimension;
    RingTopology* node_dimension;
};

}  // namespace AstraSim
