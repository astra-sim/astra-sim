/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __RENDEZVOUS_SEND_DATA_HH__
#define __RENDEZVOUS_SEND_DATA_HH__

#include "astra-sim/common/Common.hh"
#include "astra-sim/system/gpu_system/gpu_model/BasicEventHandlerData.hh"
#include "astra-sim/system/gpu_system/gpu_model/SimSendCaller.hh"
#include "astra-sim/system/gpu_system/gpu_model/Sys.hh"

namespace AstraSim {

class RendezvousSendData : public BasicEventHandlerData, public MetaData {
  public:
    RendezvousSendData(int sys_id,
                       Sys* sys,
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

#endif /* __RENDEZVOUS_SEND_DATA_HH__ */
