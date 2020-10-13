/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __QUEUELEVELS_HH__
#define __QUEUELEVELS_HH__

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
#include "QueueLevelHandler.hh"
#include "astra-sim/system/topology/RingTopology.hh"

namespace AstraSim{
    class QueueLevels{
    public:
        std::vector<QueueLevelHandler> levels;
        std::pair<int,RingTopology::Direction> get_next_queue_at_level(int level);
        std::pair<int,RingTopology::Direction> get_next_queue_at_level_first(int level);
        std::pair<int,RingTopology::Direction> get_next_queue_at_level_last(int level);
        QueueLevels(int levels, int queues_per_level,int offset);
        QueueLevels(std::vector<int> lv,int offset);
    };
}
#endif