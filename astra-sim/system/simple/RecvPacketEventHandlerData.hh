// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/common/BasicEventHandlerData.hh>
#include <astra-sim/system/simple/BaseStream.hh>

namespace AstraSim {

class Workload;
class WorkloadLayerHandlerData;

class RecvPacketEventHandlerData : public BasicEventHandlerData {
  public:
    RecvPacketEventHandlerData();
    RecvPacketEventHandlerData(BaseStream* owner,
                               int sys_id,
                               EventType event,
                               int vnet,
                               int stream_id);

    Workload* workload;
    WorkloadLayerHandlerData* wlhd;
    BaseStream* owner;
    int vnet;
    int stream_id;
    bool message_end;
    Tick ready_time;
};

}  // namespace AstraSim
