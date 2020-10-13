/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __QUEUELEVELHANDLER_HH__
#define __QUEUELEVELHANDLER_HH__

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
#include "astra-sim/system/topology/RingTopology.hh"

namespace AstraSim{
    class QueueLevelHandler{
    public:
        std::vector<int> queues;
        int allocator;
        int first_allocator;
        int last_allocator;
        int level;
        QueueLevelHandler(int level,int start,int end);
        std::pair<int,RingTopology::Direction> get_next_queue_id();
        std::pair<int,RingTopology::Direction> get_next_queue_id_first();
        std::pair<int,RingTopology::Direction> get_next_queue_id_last();
    };
}
#endif