// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/RendezvousSendData.hh>

using namespace AstraSim;

RendezvousSendData::RendezvousSendData(int sys_id,
                                       SimpleSys* sys,
                                       void* buffer,
                                       uint64_t count,
                                       int type,
                                       int dst,
                                       int tag,
                                       sim_request request,
                                       void (*msg_handler)(void* fun_arg),
                                       void* fun_arg)
    : BasicEventHandlerData(sys_id, EventType::RendezvousSend),
      send(sys,
           buffer,
           count,
           type,
           dst,
           tag,
           request,
           msg_handler,
           fun_arg,
           false) {}
