/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/SimSendCaller.hh"
#include "astra-sim/system/Sys.hh"

using namespace AstraSim;

SimSendCaller::SimSendCaller(
    Sys* sys,
    void* buffer,
    int count,
    int type,
    int dst,
    int tag,
    sim_request request,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg) {
  this->sys = sys;
  this->buffer = buffer;
  this->count = count;
  this->type = type;
  this->dst = dst;
  this->tag = tag;
  this->request = request;
  this->msg_handler = msg_handler;
  this->fun_arg = fun_arg;
}

void SimSendCaller::call(EventType type, CallData* data) {
  sys->comm_NI->sim_send(
      this->buffer,
      this->count,
      this->type,
      this->dst,
      this->tag,
      &this->request,
      this->msg_handler,
      this->fun_arg);
  delete this;
}
