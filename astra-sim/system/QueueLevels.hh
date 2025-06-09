/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __QUEUE_LEVELS_HH__
#define __QUEUE_LEVELS_HH__

#include <vector>

#include "astra-sim/common/AstraNetworkAPI.hh"
#include "astra-sim/system/QueueLevelHandler.hh"
#include "astra-sim/system/astraccl/native_collectives/logical_topology/RingTopology.hh"

namespace AstraSim {

class QueueLevels {
  public:
    std::vector<QueueLevelHandler> levels;
    std::pair<int, RingTopology::Direction> get_next_queue_at_level(int level);
    std::pair<int, RingTopology::Direction> get_next_queue_at_level_first(
        int level);
    std::pair<int, RingTopology::Direction> get_next_queue_at_level_last(
        int level);
    QueueLevels(int levels,
                int queues_per_level,
                int offset,
                AstraNetworkAPI::BackendType backend);
    QueueLevels(std::vector<int> lv,
                int offset,
                AstraNetworkAPI::BackendType backend);
};

}  // namespace AstraSim

#endif /* __QUEUE_LEVELS_HH__ */
