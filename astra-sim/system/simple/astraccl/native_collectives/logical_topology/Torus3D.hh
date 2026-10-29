// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/ComplexLogicalTopology.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/RingTopology.hh>

namespace AstraSim {

class Torus3D : public ComplexLogicalTopology {
  public:
    Torus3D(int id, int total_nodes, int local_dim, int vertical_dim);
    ~Torus3D();

    int get_num_of_dimensions() override;
    int get_num_of_nodes_in_dimension(int dimension) override;
    BasicLogicalTopology* get_basic_topology_at_dimension(
        int dimension, ComType type) override;

    RingTopology* local_dimension;
    RingTopology* vertical_dimension;
    RingTopology* horizontal_dimension;
};

}  // namespace AstraSim
