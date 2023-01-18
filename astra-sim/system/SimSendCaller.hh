/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __SIM_SEND_CALLER_HH__
#define __SIM_SEND_CALLER_HH__

#include "astra-sim/system/CallData.hh"
#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/Sys.hh"

namespace AstraSim {

class SimSendCaller : public Callable {
 public:
  void* buffer;
  int count;
  int type;
  int dst;
  int tag;
  sim_request request;
  void (*msg_handler)(void* fun_arg);
  void* fun_arg;
  void call(EventType type, CallData* data);
  Sys* sys;
  SimSendCaller(
      Sys* sys,
      void* buffer,
      int count,
      int type,
      int dst,
      int tag,
      sim_request request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg);
};

} // namespace AstraSim

#endif /* __SIM_SEND_CALLER_HH__ */
