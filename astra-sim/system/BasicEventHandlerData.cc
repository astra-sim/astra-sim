/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/BasicEventHandlerData.hh"

using namespace AstraSim;

BasicEventHandlerData::BasicEventHandlerData() {
  this->sys_id = 0;
}

BasicEventHandlerData::BasicEventHandlerData(int sys_id, EventType event) {
  this->sys_id = sys_id;
  this->event = event;
}
