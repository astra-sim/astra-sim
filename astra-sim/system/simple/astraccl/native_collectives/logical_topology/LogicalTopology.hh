// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>

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

}  // namespace AstraSim
