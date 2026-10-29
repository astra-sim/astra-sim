// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/SimSendCaller.hh>
#include <astra-sim/system/simple/SimpleSys.hh>

using namespace AstraSim;

SimSendCaller::SimSendCaller(SimpleSys* sys,
                             void* buffer,
                             int count,
                             int type,
                             int dst,
                             int tag,
                             sim_request request,
                             void (*msg_handler)(void* fun_arg),
                             void* fun_arg,
                             bool should_cleanup) {
    this->sys = sys;
    this->buffer = buffer;
    this->count = count;
    this->type = type;
    this->dst = dst;
    this->tag = tag;
    this->request = request;
    this->msg_handler = msg_handler;
    this->fun_arg = fun_arg;
    this->should_cleanup = should_cleanup;
}

void SimSendCaller::call(EventType type, CallData* data) {
    sys->comm_NI->sim_cmd(AstraNetworkAPI::CommandType::Send, this->request,
                          this->msg_handler, this->fun_arg);
    if (should_cleanup) {
        delete this;
    }
}
