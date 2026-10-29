// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/BinaryTree.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/ComplexLogicalTopology.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/RingTopology.hh>

namespace AstraSim {

class LocalRingGlobalBinaryTree : public ComplexLogicalTopology {
  public:
    LocalRingGlobalBinaryTree(int id,
                              int local_dim,
                              BinaryTree::TreeType tree_type,
                              int total_tree_nodes,
                              int start,
                              int stride);
    ~LocalRingGlobalBinaryTree();
    int get_num_of_nodes_in_dimension(int dimension) override;
    int get_num_of_dimensions() override;
    BasicLogicalTopology* get_basic_topology_at_dimension(
        int dimension, ComType type) override;

    RingTopology* local_dimension;
    RingTopology* global_dimension_other;
    BinaryTree* global_dimension_all_reduce;
};

}  // namespace AstraSim
