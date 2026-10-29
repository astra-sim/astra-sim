// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/common/BasicEventHandlerData.hh>

namespace AstraSim {

class Workload;
class WorkloadLayerHandlerData;

class MemEventHandlerData : public BasicEventHandlerData {
  public:
    MemEventHandlerData();
    Workload* workload;
    WorkloadLayerHandlerData* wlhd;
};

}  // namespace AstraSim
