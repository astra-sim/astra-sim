// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/AstraSimDataAPI.hh>
#include <astra-sim/system/common/CallData.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/simple/BaseStream.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/LogicalTopology.hh>
#include <cmath>
#include <iostream>
#include <utility>

namespace AstraSim {
struct WFReq;
class Algorithm : public Callable {
  public:
    enum class Name { Ring = 0, DoubleBinaryTree, AllToAll, HalvingDoubling };

    Algorithm();
    virtual ~Algorithm() = default;
    virtual void run(EventType event, CallData* data) = 0;
    virtual void init(BaseStream* stream);
    virtual void call(EventType event, CallData* data);
    virtual void exit();

    Name name;
    int id;
    BaseStream* stream;
    LogicalTopology* logical_topo;
    uint64_t data_size;
    uint64_t final_data_size;
    ComType comType;
    bool enabled;

    BaseStream* curr_stream;
    uint64_t collectiveSize;
    uint64_t collectiveStartAddr;
};

}  // namespace AstraSim
