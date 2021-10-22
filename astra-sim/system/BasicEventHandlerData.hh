/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __BASICEVENTHANDLERDATA_HH__
#define __BASICEVENTHANDLERDATA_HH__

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
#include "CallData.hh"
#include "Common.hh"

namespace AstraSim {
class BasicEventHandlerData : public CallData {
 public:
  int nodeId;
  EventType event;
  BasicEventHandlerData(int nodeId, EventType event);
};
} // namespace AstraSim
#endif