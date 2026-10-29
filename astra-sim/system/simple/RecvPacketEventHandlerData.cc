// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/RecvPacketEventHandlerData.hh>

#include <astra-sim/system/common/Sys.hh>
#include <astra-sim/system/common/WorkloadLayerHandlerData.hh>

using namespace AstraSim;

RecvPacketEventHandlerData::RecvPacketEventHandlerData() {
    this->wlhd = nullptr;
    this->owner = nullptr;
}

RecvPacketEventHandlerData::RecvPacketEventHandlerData(
    BaseStream* owner, int sys_id, EventType event, int vnet, int stream_id)
    : BasicEventHandlerData(sys_id, event) {
    this->workload = nullptr;
    this->wlhd = nullptr;
    this->owner = owner;
    this->vnet = vnet;
    this->stream_id = stream_id;
    this->message_end = true;
    ready_time = Sys::boostedTick();
}
