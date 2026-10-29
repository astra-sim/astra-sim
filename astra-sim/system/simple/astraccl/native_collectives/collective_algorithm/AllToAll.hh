// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/common/CallData.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/collective_algorithm/Ring.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/RingTopology.hh>

namespace AstraSim {

class AllToAll : public Ring {
  public:
    AllToAll(ComType type,
             int window,
             int id,
             RingTopology* allToAllTopology,
             uint64_t data_size,
             RingTopology::Direction direction,
             InjectionPolicy injection_policy);
    void run(EventType event, CallData* data);
    void process_max_count();
    int get_non_zero_latency_packets();
    void* genRecvPacketEventHandlerData();

    int middle_point;
};

}  // namespace AstraSim
