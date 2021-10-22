/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "SendPacketEventHandlerData.hh"
namespace AstraSim {
SendPacketEventHandlerData::SendPacketEventHandlerData(
    int senderNodeId,
    int receiverNodeId,
    int tag)
    : BasicEventHandlerData(senderNodeId, EventType::PacketSent) {
  this->receiverNodeId = receiverNodeId;
  this->tag = tag;
}
} // namespace AstraSim