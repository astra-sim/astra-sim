/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __RECVPACKETEVENTHADNDLERDATA_HH__
#define __RECVPACKETEVENTHADNDLERDATA_HH__

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
#include "BaseStream.hh"
#include "BasicEventHandlerData.hh"

namespace AstraSim {
class RecvPacketEventHadndlerData : public BasicEventHandlerData,
                                    public MetaData {
 public:
  BaseStream* owner;
  int vnet;
  int stream_num;
  bool message_end;
  Tick ready_time;
  RecvPacketEventHadndlerData(
      BaseStream* owner,
      int nodeId,
      EventType event,
      int vnet,
      int stream_num);
};
} // namespace AstraSim
#endif