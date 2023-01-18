/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/RendezvousRecvData.hh"

using namespace AstraSim;

RendezvousRecvData::RendezvousRecvData(
    int sys_id,
    Sys* sys,
    void* buffer,
    uint64_t count,
    int type,
    int src,
    int tag,
    sim_request request,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg)
    : BasicEventHandlerData(sys_id, EventType::RendezvousRecv) {
  this->recv = new SimRecvCaller(
      sys, buffer, count, type, src, tag, request, msg_handler, fun_arg);
}
