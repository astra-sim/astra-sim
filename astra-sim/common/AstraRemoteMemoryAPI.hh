// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cstdint>

namespace AstraSim {

class Sys;
class WorkloadLayerHandlerData;

class AstraRemoteMemoryAPI {
  public:
    virtual ~AstraRemoteMemoryAPI() = default;
    virtual void set_sys(int id, Sys* sys) = 0;
    virtual void issue(uint64_t tensor_size,
                       WorkloadLayerHandlerData* wlhd) = 0;
};

}  // namespace AstraSim
