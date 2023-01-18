/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __SIM_RECV_CALLER_HH__
#define __SIM_RECV_CALLER_HH__

#include "astra-sim/system/CallData.hh"
#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/Sys.hh"

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
  void call(EventType type, CallData* data);
  Sys* sys;
  SimRecvCaller(
      Sys* sys,
      void* buffer,
      int count,
      int type,
      int src,
      int tag,
      sim_request request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg);
};

} // namespace AstraSim

#endif /* __SIM_RECV_CALLER_HH__ */
