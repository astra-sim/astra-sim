// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/BasicEventHandlerData.hh>
#include <astra-sim/system/simple/SimRecvCaller.hh>
#include <astra-sim/system/simple/SimpleSys.hh>

namespace AstraSim {

class RendezvousRecvData : public BasicEventHandlerData, public MetaData {
  public:
    RendezvousRecvData(int sys_id,
                       SimpleSys* sys,
                       void* buffer,
                       uint64_t count,
                       int type,
                       int src,
                       int tag,
                       sim_request request,
                       void (*msg_handler)(void* fun_arg),
                       void* fun_arg);
    SimRecvCaller recv;
};

}  // namespace AstraSim
