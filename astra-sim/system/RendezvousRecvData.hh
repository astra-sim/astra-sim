/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __RENDEZVOUS_RECV_DATA_HH__
#define __RENDEZVOUS_RECV_DATA_HH__

#include "astra-sim/system/BasicEventHandlerData.hh"
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/SimRecvCaller.hh"
#include "astra-sim/system/Sys.hh"

namespace AstraSim {

class RendezvousRecvData : public BasicEventHandlerData, public MetaData {
 public:
  RendezvousRecvData(
      int sys_id,
      Sys* sys,
      void* buffer,
      uint64_t count,
      int type,
      int src,
      int tag,
      sim_request request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg);
  SimRecvCaller* recv;
};

} // namespace AstraSim

#endif /* __RENDEZVOUS_RECV_DATA_HH__ */
