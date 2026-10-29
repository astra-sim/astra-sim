// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/common/BasicEventHandlerData.hh>
#include <astra-sim/system/gpu/ComputeUnit.hh>

namespace AstraSim {

class ComputeUnitPacketEventHandlerData : public BasicEventHandlerData {
  public:
    ComputeUnitPacketEventHandlerData(
        int sys_id,
        EventType event,
        ComputeUnit* _cu_p,
        ComputeUnit::CU_Event_CallData* _cu_event_calldata,
        EventType _cu_event_type);
    ComputeUnit* cu_p;
    ComputeUnit::CU_Event_CallData* cu_event_calldata;
    EventType cu_event_type;
};

}  // end of namespace AstraSim
