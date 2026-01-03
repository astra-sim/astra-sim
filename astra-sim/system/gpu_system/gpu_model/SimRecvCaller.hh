/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __SIM_RECV_CALLER_HH__
#define __SIM_RECV_CALLER_HH__

#include "astra-sim/common/Common.hh"
#include "astra-sim/system/gpu_system/gpu_model/CallData.hh"
#include "astra-sim/system/gpu_system/gpu_model/Callable.hh"
#include "astra-sim/system/gpu_system/gpu_model/Sys.hh"

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
    bool should_cleanup;
    void call(EventType type, CallData* data);
    Sys* sys;
    SimRecvCaller(Sys* sys,
                  void* buffer,
                  int count,
                  int type,
                  int src,
                  int tag,
                  sim_request request,
                  void (*msg_handler)(void* fun_arg),
                  void* fun_arg,
                  bool should_cleanup);
};

}  // namespace AstraSim

#endif /* __SIM_RECV_CALLER_HH__ */
