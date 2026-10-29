// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/LogicalTopology.hh>

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

}  // namespace AstraSim
