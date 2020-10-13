/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __BASICEVENTHANDLERDATA_HH__
#define __BASICEVENTHANDLERDATA_HH__

#include <map>
#include <math.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <tuple>
#include <cstdint>
#include <list>
#include <vector>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <assert.h>
#include "Common.hh"
#include "CallData.hh"

namespace AstraSim{
    class BasicEventHandlerData:public CallData{
    public:
        int nodeId;
        EventType event;
        BasicEventHandlerData(int nodeId, EventType event);
    };
}
#endif