// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <string>

#include <astra-sim/common/Common.hh>

namespace AstraSim {

class SimObject {
  public:
    virtual ~SimObject() = default;
    virtual const std::string& name() const = 0;
    virtual Tick curTick() const = 0;
};

}  // end of namespace AstraSim
