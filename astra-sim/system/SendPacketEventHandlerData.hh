/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __SEND_PACKET_EVENT_HANDLER_DATA_HH__
#define __SEND_PACKET_EVENT_HANDLER_DATA_HH__

#include "astra-sim/system/BasicEventHandlerData.hh"
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/WorkloadLayerHandlerData.hh"

namespace AstraSim {

class SendPacketEventHandlerData : public BasicEventHandlerData{
 public:
  int tag;
  Callable *callable;
  WorkloadLayerHandlerData* wlhd;
  SendPacketEventHandlerData();
  SendPacketEventHandlerData(Callable *callable, int tag);
};

} // namespace AstraSim

#endif /* __SEND_PACKET_EVENT_HANDLER_DATA_HH__ */
