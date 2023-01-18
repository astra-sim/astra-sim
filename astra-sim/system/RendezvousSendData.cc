/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/RendezvousSendData.hh"

using namespace AstraSim;

RendezvousSendData::RendezvousSendData(
    int sys_id,
    Sys* sys,
    void* buffer,
    uint64_t count,
    int type,
    int dst,
    int tag,
    sim_request request,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg)
    : BasicEventHandlerData(sys_id, EventType::RendezvousSend) {
  this->send = new SimSendCaller(
      sys, buffer, count, type, dst, tag, request, msg_handler, fun_arg);
}
