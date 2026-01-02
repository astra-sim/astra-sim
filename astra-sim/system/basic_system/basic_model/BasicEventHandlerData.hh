/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __BASIC_EVENT_HANDLER_DATA_HH__
#define __BASIC_EVENT_HANDLER_DATA_HH__

#include "astra-sim/common/Common.hh"
#include "astra-sim/system/basic_system/basic_model/CallData.hh"

namespace AstraSim {

class BasicEventHandlerData : public CallData {
  public:
    BasicEventHandlerData();
    BasicEventHandlerData(int sys_id, EventType event);

    int sys_id;
    EventType event;
};

}  // namespace AstraSim

#endif /* __BASIC_EVENT_HANDLER_DATA_HH__ */
