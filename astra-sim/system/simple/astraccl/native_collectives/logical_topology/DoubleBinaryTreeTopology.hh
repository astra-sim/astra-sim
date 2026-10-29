// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/BinaryTree.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/ComplexLogicalTopology.hh>

namespace AstraSim {

class DoubleBinaryTreeTopology : public ComplexLogicalTopology {
  public:
    DoubleBinaryTreeTopology(int id,
                             int total_tree_nodes,
                             int start,
                             int stride);
    ~DoubleBinaryTreeTopology();
    LogicalTopology* get_topology() override;
    BasicLogicalTopology* get_basic_topology_at_dimension(
        int dimension, ComType type) override;
    int get_num_of_dimensions() override;
    int get_num_of_nodes_in_dimension(int dimension) override;

    int counter;
    BinaryTree* DBMAX;
    BinaryTree* DBMIN;
};

}  // namespace AstraSim
