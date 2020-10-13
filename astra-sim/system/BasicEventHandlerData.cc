/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "BasicEventHandlerData.hh"
namespace AstraSim {
BasicEventHandlerData::BasicEventHandlerData(int nodeId, EventType event) {
  this->nodeId = nodeId;
  this->event = event;
}
} // namespace AstraSim
