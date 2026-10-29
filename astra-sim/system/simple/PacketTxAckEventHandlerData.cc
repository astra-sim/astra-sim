// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/PacketTxAckEventHandlerData.hh>

using namespace AstraSim;

PacketTxAckEventHandlerData::PacketTxAckEventHandlerData() {
    workload = nullptr;
    wlhd = nullptr;
    owner = nullptr;
}

PacketTxAckEventHandlerData::PacketTxAckEventHandlerData(BaseStream* owner)
    : BasicEventHandlerData(-1, EventType::PacketTxAck) {
    this->owner = owner;

    // set these after construction through direct access
    this->workload = nullptr;
    this->wlhd = nullptr;
}
