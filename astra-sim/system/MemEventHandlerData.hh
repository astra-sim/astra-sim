/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __MEM_EVENT_HANDLER_DATA_HH__
#define __MEM_EVENT_HANDLER_DATA_HH__

#include "astra-sim/system/BasicEventHandlerData.hh"

namespace AstraSim {

class Workload;
class WorkloadLayerHandlerData;

class MemEventHandlerData : public BasicEventHandlerData{
 public:
  MemEventHandlerData();
  Workload* workload;
  WorkloadLayerHandlerData* wlhd;
};

} // namespace AstraSim

#endif /* __MEM_EVENT_HANDLER_DATA_HH__ */
