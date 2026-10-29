// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/BasicEventHandlerData.hh>
#include <astra-sim/system/simple/SimSendCaller.hh>
#include <astra-sim/system/simple/SimpleSys.hh>

namespace AstraSim {

class RendezvousSendData : public BasicEventHandlerData, public MetaData {
  public:
    RendezvousSendData(int sys_id,
                       SimpleSys* sys,
                       void* buffer,
                       uint64_t count,
                       int type,
                       int dst,
                       int tag,
                       sim_request request,
                       void (*msg_handler)(void* fun_arg),
                       void* fun_arg);
    SimSendCaller send;
};

}  // namespace AstraSim
