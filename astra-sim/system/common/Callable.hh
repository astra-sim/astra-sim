// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/CallData.hh>

namespace AstraSim {

class Callable {
  public:
    virtual ~Callable() = default;
    virtual void call(EventType type, CallData* data) = 0;
};

}  // namespace AstraSim
