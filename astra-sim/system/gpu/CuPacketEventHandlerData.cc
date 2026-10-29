// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/gpu/CuPacketEventHandlerData.hh>

using namespace AstraSim;

ComputeUnitPacketEventHandlerData::ComputeUnitPacketEventHandlerData(
    int sys_id,
    EventType event,
    ComputeUnit* _cu_p,
    ComputeUnit::CU_Event_CallData* _cu_event_calldata,
    EventType _cu_event_type)
    : BasicEventHandlerData(sys_id, event),
      cu_p(_cu_p),
      cu_event_calldata(_cu_event_calldata),
      cu_event_type(_cu_event_type) {}
