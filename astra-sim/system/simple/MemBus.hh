// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <string>

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/Callable.hh>

namespace AstraSim {

class SimpleSys;
class LogGP;
class MemBus {
  public:
    enum class Transmition { Fast, Usual };

    MemBus(std::string side1,
           std::string side2,
           SimpleSys* sys,
           Tick L,
           Tick o,
           Tick g,
           double G,
           bool model_shared_bus,
           int communication_delay,
           bool attach);
    ~MemBus();

    void send_from_NPU_to_MA(Transmition transmition,
                             int bytes,
                             bool processed,
                             bool send_back,
                             Callable* callable);
    void send_from_MA_to_NPU(Transmition transmition,
                             int bytes,
                             bool processed,
                             bool send_back,
                             Callable* callable);

    LogGP* NPU_side;
    LogGP* MA_side;
    SimpleSys* sys;
    int communication_delay;
    bool model_shared_bus;
};

}  // namespace AstraSim
