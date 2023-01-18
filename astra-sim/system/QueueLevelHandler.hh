/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __QUEUE_LEVEL_HANDLER_HH__
#define __QUEUE_LEVEL_HANDLER_HH__

#include <vector>

#include "astra-sim/system/AstraNetworkAPI.hh"
#include "astra-sim/system/topology/RingTopology.hh"

namespace AstraSim {

class QueueLevelHandler {
 public:
  QueueLevelHandler(
      int level,
      int start,
      int end,
      AstraNetworkAPI::BackendType backend);
  std::pair<int, RingTopology::Direction> get_next_queue_id();
  std::pair<int, RingTopology::Direction> get_next_queue_id_first();
  std::pair<int, RingTopology::Direction> get_next_queue_id_last();

  std::vector<int> queues;
  int allocator;
  int first_allocator;
  int last_allocator;
  int level;
  AstraNetworkAPI::BackendType backend;
};

} // namespace AstraSim

#endif /* __QUEUE_LEVEL_HANDLER_HH__ */
