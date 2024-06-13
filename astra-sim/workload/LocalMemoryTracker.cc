#include "astra-sim/workload/LocalMemoryTracker.hh"

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include "astra-sim/common/Common.hh"
#include "astra-sim/common/Logging.hh"
#include "astra-sim/system/Sys.hh"
#include "et_feeder/et_feeder.h"
#include "et_feeder/et_feeder_node.h"

using namespace AstraSim;

LocalMemoryTracker::LocalMemoryTracker(
    Workload* workload,
    bool trackMemoryActivities) {
  this->workload = workload;
  this->trackMemoryActivities = trackMemoryActivities;
  this->memoryContentSize = 0ul;
  this->peakMemoryUsage = 0ul;
}

void LocalMemoryTracker::issueNode(
    Chakra::ETFeeder* etFeeder,
    std::shared_ptr<Chakra::ETFeederNode> node) {
  Tick now = Sys::boostedTick();
  size_t tensorSize = node->tensor_size();
  this->memoryContentSize += tensorSize;
  memoryUsageTimeline.insert({now, this->memoryContentSize});
  if (this->trackMemoryActivities) {
    MemoryActivity writeActivity = {MemoryActivityType::WRITE_START, node, now};
    this->memoryActivities.push_back(writeActivity);
    MemoryActivity allocActivity = {MemoryActivityType::ALLOC, node, now};
    this->memoryActivities.push_back(allocActivity);
    for (auto parentId : node->getChakraNode()->data_deps()) {
      auto parent = etFeeder->lookupNode(parentId);
      MemoryActivity readActivity = {
          MemoryActivityType::READ_START, parent, now};
      this->memoryActivities.push_back(readActivity);
    }
  }
  if (this->memoryContentSize > this->peakMemoryUsage)
    this->peakMemoryUsage = this->memoryContentSize;
}

void LocalMemoryTracker::finishedNode(
    Chakra::ETFeeder* etFeeder,
    std::shared_ptr<Chakra::ETFeederNode> node) {
  Tick now = Sys::boostedTick();
  if (this->trackMemoryActivities) {
    MemoryActivity writeActivity = {MemoryActivityType::WRITE_END, node, now};
    this->memoryActivities.push_back(writeActivity);
    for (auto parentId : node->getChakraNode()->data_deps()) {
      auto parent = etFeeder->lookupNode(parentId);
      MemoryActivity readActivity = {MemoryActivityType::READ_END, parent, now};
      this->memoryActivities.push_back(readActivity);
    }
  }

  releasedNodes.emplace(node);
  for (auto parentId : node->getChakraNode()->data_deps()) {
    auto parent = etFeeder->lookupNode(parentId);
    bool depResolved = true;
    for (auto child : parent->getChildren())
      if (releasedNodes.count(child) == 0) {
        depResolved = false;
        break;
      }
    if (depResolved) {
      this->memoryContentSize -= parent->tensor_size();
      this->memoryUsageTimeline.insert({now, this->memoryContentSize});
      if (this->trackMemoryActivities) {
        MemoryActivity freeActivity = {MemoryActivityType::FREE, node, now};
        this->memoryActivities.push_back(freeActivity);
      }
    }
  }
}

void LocalMemoryTracker::report(std::string reportPath) {
  auto logger = LoggerFactory::get_logger("local-memory-tracker");
  logger->info(
      "sys->id={}, peak memory usage={}",
      this->workload->sys->id,
      this->peakMemoryUsage);
}
