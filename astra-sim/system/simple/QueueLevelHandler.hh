// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <vector>

#include <astra-sim/common/AstraNetworkAPI.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/RingTopology.hh>

namespace AstraSim {

class QueueLevelHandler {
  public:
    QueueLevelHandler(int level,
                      int start,
                      int end,
                      AstraNetworkAPI::BackendType backend);
    std::pair<int, RingTopology::Direction> get_next_queue_id();
    std::pair<int, RingTopology::Direction> get_next_queue_id_first();
    std::pair<int, RingTopology::Direction> get_next_queue_id_last();

    std::vector<int> queues;
    int allocator;
    int first_allocator;
    int last_allocator;
    int level;
    AstraNetworkAPI::BackendType backend;
};

}  // namespace AstraSim
