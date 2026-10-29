// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

//
// Created by Saeed Rashidi on 7/7/22.
//

#pragma once

#include <vector>

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/LogicalTopology.hh>

namespace AstraSim {

class CollectivePlan {
  public:
    LogicalTopology* topology;
    std::vector<CollectiveImpl*> implementation_per_dimension;
    std::vector<bool> dimensions_involved;
    bool should_be_removed;
    CollectivePlan(LogicalTopology* topology,
                   std::vector<CollectiveImpl*> implementation_per_dimension,
                   std::vector<bool> dimensions_involved,
                   bool should_be_removed);
    ~CollectivePlan();
};

}  // namespace AstraSim
