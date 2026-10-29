// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/CallData.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/simple/SimpleSys.hh>

namespace AstraSim {

class SimRecvCaller : public Callable {
  public:
    void* buffer;
    int count;
    int type;
    int src;
    int tag;
    sim_request request;
    void (*msg_handler)(void* fun_arg);
    void* fun_arg;
    bool should_cleanup;
    void call(EventType type, CallData* data);
    SimpleSys* sys;
    SimRecvCaller(SimpleSys* sys,
                  void* buffer,
                  int count,
                  int type,
                  int src,
                  int tag,
                  sim_request request,
                  void (*msg_handler)(void* fun_arg),
                  void* fun_arg,
                  bool should_cleanup);
};

}  // namespace AstraSim
