// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/RendezvousRecvData.hh>

using namespace AstraSim;

RendezvousRecvData::RendezvousRecvData(int sys_id,
                                       SimpleSys* sys,
                                       void* buffer,
                                       uint64_t count,
                                       int type,
                                       int src,
                                       int tag,
                                       sim_request request,
                                       void (*msg_handler)(void* fun_arg),
                                       void* fun_arg)
    : BasicEventHandlerData(sys_id, EventType::RendezvousRecv),
      recv(sys,
           buffer,
           count,
           type,
           src,
           tag,
           request,
           msg_handler,
           fun_arg,
           false) {}
