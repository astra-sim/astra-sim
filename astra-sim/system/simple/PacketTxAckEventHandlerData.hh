// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/common/BasicEventHandlerData.hh>
#include <astra-sim/system/common/WorkloadLayerHandlerData.hh>
#include <astra-sim/system/simple/BaseStream.hh>

namespace AstraSim {

class PacketTxAckEventHandlerData : public BasicEventHandlerData {
  public:
    BaseStream* owner;
    Workload* workload;
    WorkloadLayerHandlerData* wlhd;
    PacketTxAckEventHandlerData();
    PacketTxAckEventHandlerData(BaseStream* owner);
};

}  // namespace AstraSim
