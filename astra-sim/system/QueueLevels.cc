/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/QueueLevels.hh"

using namespace AstraSim;

QueueLevels::QueueLevels(
    int total_levels,
    int queues_per_level,
    int offset,
    AstraNetworkAPI::BackendType backend) {
  int start = offset;
  for (int i = 0; i < total_levels; i++) {
    QueueLevelHandler tmp(i, start, start + queues_per_level - 1, backend);
    levels.push_back(tmp);
    start += queues_per_level;
  }
}

QueueLevels::QueueLevels(
    std::vector<int> lv,
    int offset,
    AstraNetworkAPI::BackendType backend) {
  int start = offset;
  int l = 0;
  for (auto& i : lv) {
    QueueLevelHandler tmp(l++, start, start + i - 1, backend);
    levels.push_back(tmp);
    start += i;
  }
}

std::pair<int, RingTopology::Direction> QueueLevels::get_next_queue_at_level(
    int level) {
  return levels[level].get_next_queue_id();
}

std::pair<int, RingTopology::Direction> QueueLevels::
    get_next_queue_at_level_first(int level) {
  return levels[level].get_next_queue_id_first();
}

std::pair<int, RingTopology::Direction> QueueLevels::
    get_next_queue_at_level_last(int level) {
  return levels[level].get_next_queue_id_last();
}
