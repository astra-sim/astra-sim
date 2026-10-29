// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/AstraNetworkAPI.hh>
#include <astra-sim/system/common/BasicEventHandlerData.hh>

namespace AstraSim {

class Workload;

class WorkloadLayerHandlerData : public BasicEventHandlerData, public MetaData {
  public:
    Workload* workload;
    uint64_t node_id;
    WorkloadLayerHandlerData();
    WorkloadLayerHandlerData(int sys_id,
                             Workload* _wl,
                             uint64_t _node_id,
                             EventType _event);
};

}  // namespace AstraSim
