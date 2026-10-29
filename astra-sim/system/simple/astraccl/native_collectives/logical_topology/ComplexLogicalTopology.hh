// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/LogicalTopology.hh>

namespace AstraSim {

class ComplexLogicalTopology : public LogicalTopology {
  public:
    ComplexLogicalTopology() {}
    virtual ~ComplexLogicalTopology() = default;

    virtual int get_num_of_dimensions() override = 0;
    virtual int get_num_of_nodes_in_dimension(int dimension) override = 0;
    virtual BasicLogicalTopology* get_basic_topology_at_dimension(
        int dimension, ComType type) override = 0;
};

}  // namespace AstraSim
