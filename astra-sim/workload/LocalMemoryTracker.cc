#include "astra-sim/workload/LocalMemoryTracker.hh"
#include <algorithm>
#include "astra-sim/system/Sys.hh"
#include "astra-sim/workload/Workload.hh"
#include "extern/graph_frontend/chakra/src/feeder_v2/et_feeder.h"
#include "extern/graph_frontend/chakra/src/feeder_v2/et_feeder_node.h"

using namespace AstraSim;

LocalMemoryTracker::LocalMemoryTracker(
    Statistics* statistics,
    Chakra::ETFeeder* et_feeder)
    : statistics(statistics), et_feeder(et_feeder) {
  readWriteActivities.clear();
  allocFreeActivities.clear();
}

void LocalMemoryTracker::extractMemoryActivities() {
  this->readWriteActivities.clear();
  for (const auto& [node_id, operator_statistics] :
       statistics->get_operator_statistics()) {
    if (operator_statistics.type ==
        Statistics::OperatorStatistics::OperatorType::REMOTE_MEM) {
      Sys::sys_panic(
          "REMOTE_MEM offloading/prefetching not supported yet by LocalMemoryTracker");
    }
    if (readWriteActivities.find(node_id) == readWriteActivities.end()) {
      readWriteActivities[node_id] = std::vector<MemoryActivity>();
    }
    const std::shared_ptr<Chakra::ETFeederNode> node =
        et_feeder->lookupNode(node_id);

    // write
    MemoryActivity write_start = {
        MemoryActivityType::WRITE_START,
        node_id,
        node_id,
        operator_statistics.start_time};
    MemoryActivity write_end = {
        MemoryActivityType::WRITE_END,
        node_id,
        node_id,
        operator_statistics.end_time};
    readWriteActivities[node_id].push_back(write_start);
    readWriteActivities[node_id].push_back(write_end);

    // read
    const auto data_deps_ids = node->getChakraNode()->data_deps();
    for (const auto& data_dep_id : data_deps_ids) {
      if (readWriteActivities.find(data_dep_id) == readWriteActivities.end())
        readWriteActivities[data_dep_id] = std::vector<MemoryActivity>();
      MemoryActivity read_start = {
          MemoryActivityType::READ_START,
          data_dep_id,
          node_id,
          operator_statistics.start_time};
      MemoryActivity read_end = {
          MemoryActivityType::READ_END,
          data_dep_id,
          node_id,
          operator_statistics.end_time};
      readWriteActivities[data_dep_id].push_back(read_start);
      readWriteActivities[data_dep_id].push_back(read_end);
    }
  }
  for (auto& [node_id, activities] : this->readWriteActivities) {
    std::sort(
        activities.begin(),
        activities.end(),
        [](const MemoryActivity& a, const MemoryActivity& b) {
          return a.tick < b.tick;
        });
    MemoryActivity alloc = {
        MemoryActivityType::ALLOC, node_id, node_id, activities.front().tick};
    MemoryActivity free = {
        MemoryActivityType::FREE, node_id, node_id, activities.back().tick};
    this->allocFreeActivities.push_back(alloc);
    this->allocFreeActivities.push_back(free);
  }
  std::sort(
      this->allocFreeActivities.begin(),
      this->allocFreeActivities.end(),
      [](const MemoryActivity& a, const MemoryActivity& b) {
        return a.tick < b.tick;
      });
}

const std::
    unordered_map<NodeId, std::vector<LocalMemoryTracker::MemoryActivity>>&
    LocalMemoryTracker::getReadWriteActivities() const {
  return readWriteActivities;
}

const std::vector<LocalMemoryTracker::MemoryActivity>& LocalMemoryTracker::
    getAllocFreeActivities() const {
  return allocFreeActivities;
}

void LocalMemoryTracker::extractMemoryUsage() {
  memory_usage.clear();
  uint64_t current_memory_usage = 0ul;
  for (const auto& activity : allocFreeActivities) {
    uint64_t tensor_size =
        this->et_feeder->lookupNode(activity.dataOwner)->tensor_size(0ul);
    if (tensor_size == 0ul)
      LoggerFactory::get_logger("local_memory_tracker")
          ->debug("Node {} has tensor size 0", activity.dataOwner);
    if (activity.type == MemoryActivityType::ALLOC)
      current_memory_usage += tensor_size;
    else if (activity.type == MemoryActivityType::FREE)
      current_memory_usage -= tensor_size;
    memory_usage[activity.tick] = current_memory_usage;
  }
}

const std::map<Tick, uint64_t>& LocalMemoryTracker::getMemoryUsage() const {
  return memory_usage;
}

void LocalMemoryTracker::extractAvgPeakUsage() {
  double weighted_average_memory_usage = 0.0;
  uint64_t average_memory_usage;
  Tick total_time = 0;
  Tick& last_tick = total_time;
  uint64_t peak_memory_usage = 0ul;

  for (const auto& [tick, usage] : this->getMemoryUsage()) {
    weighted_average_memory_usage +=
        static_cast<double>(usage) * static_cast<double>(tick - last_tick);
    peak_memory_usage = std::max(peak_memory_usage, usage);
    total_time = tick;
  }
  average_memory_usage = static_cast<uint64_t>(
      weighted_average_memory_usage / static_cast<double>(total_time));
  this->average_memory_usage = average_memory_usage;
  this->peak_memory_usage = peak_memory_usage;
}

const uint64_t LocalMemoryTracker::getAverageMemoryUsage() const {
  return average_memory_usage;
}

const uint64_t LocalMemoryTracker::getPeakMemoryUsage() const {
  return peak_memory_usage;
}
