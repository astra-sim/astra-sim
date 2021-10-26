/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef ASTRA_SIM_SENDPACKETEVENTHANDLERDATA_H
#define ASTRA_SIM_SENDPACKETEVENTHANDLERDATA_H

class SendPacketEventHandlerData {};

#include <assert.h>
#include <math.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <list>
#include <map>
#include <sstream>
#include <tuple>
#include <vector>
#include "ns3/BaseStream.hh"
#include "ns3/BasicEventHandlerData.hh"
#include "ns3/Common.hh"
#include "ns3/Sys.hh"

namespace AstraSim {
class SendPacketEventHandlerData : public BasicEventHandlerData,
                                   public MetaData {
 public:
  int receiverNodeId;
  int tag;
  SendPacketEventHandlerData(int senderNodeId, int receiverNodeId, int tag);
};
} // namespace AstraSim
#endif // ASTRA_SIM_SENDPACKETEVENTHANDLERDATA_H
