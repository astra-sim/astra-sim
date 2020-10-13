/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "RecvPacketEventHadndlerData.hh"
namespace AstraSim {
RecvPacketEventHadndlerData::RecvPacketEventHadndlerData(
    BaseStream* owner,
    int nodeId,
    EventType event,
    int vnet,
    int stream_num)
    : BasicEventHandlerData(nodeId, event) {
  this->owner = owner;
  this->vnet = vnet;
  this->stream_num = stream_num;
  this->message_end = true;
  ready_time = Sys::boostedTick();
  // std::cout<<"################## instantiated with nodeID:
  // "<<this->nodeId<<std::endl;
}
} // namespace AstraSim