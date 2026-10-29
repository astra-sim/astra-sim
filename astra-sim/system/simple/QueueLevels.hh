// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <vector>

#include <astra-sim/common/AstraNetworkAPI.hh>
#include <astra-sim/system/simple/QueueLevelHandler.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/RingTopology.hh>

namespace AstraSim {

class QueueLevels {
  public:
    std::vector<QueueLevelHandler> levels;
    std::pair<int, RingTopology::Direction> get_next_queue_at_level(int level);
    std::pair<int, RingTopology::Direction> get_next_queue_at_level_first(
        int level);
    std::pair<int, RingTopology::Direction> get_next_queue_at_level_last(
        int level);
    QueueLevels(int levels,
                int queues_per_level,
                int offset,
                AstraNetworkAPI::BackendType backend);
    QueueLevels(std::vector<int> lv,
                int offset,
                AstraNetworkAPI::BackendType backend);
};

}  // namespace AstraSim
