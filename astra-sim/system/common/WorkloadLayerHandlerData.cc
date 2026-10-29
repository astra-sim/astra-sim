// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/common/WorkloadLayerHandlerData.hh>

using namespace AstraSim;

WorkloadLayerHandlerData::WorkloadLayerHandlerData() {
    node_id = 0;
}

WorkloadLayerHandlerData::WorkloadLayerHandlerData(int sys_id,
                                                   Workload* _wl,
                                                   uint64_t _node_id,
                                                   EventType event)
    : BasicEventHandlerData(sys_id, event),
      workload(_wl),
      node_id(_node_id) {}
