#include "astra-sim/workload/LocalMemoryTracker.hh"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include "astra-sim/common/Common.hh"
#include "astra-sim/common/Logging.hh"
#include "astra-sim/system/Sys.hh"
#include "extern/graph_frontend/chakra/src/feeder_v2/et_feeder.h"
#include "extern/graph_frontend/chakra/src/feeder_v2/et_feeder_node.h"
#include "json/json.hpp"

using namespace AstraSim;
using json = nlohmann::json;

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
  // if (this->memoryContentSize > 1e12)
  //   Sys::sys_panic("memory content size exceeded 1TB");
  Tick now = Sys::boostedTick();
  size_t tensorSize = node->tensor_size(0ul);
  if (node->type() == ChakraProtoMsg::NodeType::COMM_RECV_NODE)
    return;
  size_t old_memoryContentSize = this->memoryContentSize;
  this->memoryContentSize += tensorSize;
  // if (this->memoryContentSize > 1e12)
  //   Sys::sys_panic("memory content size exceeded 1TB");
  memoryUsageTimeline.insert({now, this->memoryContentSize});
  if (this->trackMemoryActivities) {
    MemoryActivity writeActivity = {
        MemoryActivityType::WRITE_START, node, node, now};
    this->memoryActivities.push_back(writeActivity);
    MemoryActivity allocActivity = {MemoryActivityType::ALLOC, node, node, now};
    this->memoryActivities.push_back(allocActivity);
    for (auto parentId : node->getChakraNode()->data_deps()) {
      auto parent = etFeeder->lookupNode(parentId);
      MemoryActivity readActivity = {
          MemoryActivityType::READ_START, parent, node, now};
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
  size_t tensorSize = node->tensor_size(0ul);
  if (node->type() == ChakraProtoMsg::NodeType::COMM_RECV_NODE) {
    this->memoryContentSize += tensorSize;
    // if (this->memoryContentSize > 1e12)
    //   Sys::sys_panic("memory content size exceeded 1TB");
    memoryUsageTimeline.insert({now, this->memoryContentSize});
    if (this->trackMemoryActivities) {
      MemoryActivity writeActivity = {
          MemoryActivityType::WRITE_START, node, node, now - 100};
      this->memoryActivities.push_back(writeActivity);
      MemoryActivity allocActivity = {
          MemoryActivityType::ALLOC, node, node, now - 100};
      this->memoryActivities.push_back(allocActivity);
      for (auto parentId : node->getChakraNode()->data_deps()) {
        auto parent = etFeeder->lookupNode(parentId);
        MemoryActivity readActivity = {
            MemoryActivityType::READ_START, parent, node, now};
        this->memoryActivities.push_back(readActivity);
      }
    }
    if (this->memoryContentSize > this->peakMemoryUsage)
      this->peakMemoryUsage = this->memoryContentSize;
  }
  if (this->trackMemoryActivities) {
    MemoryActivity writeActivity = {
        MemoryActivityType::WRITE_END, node, node, now};
    this->memoryActivities.push_back(writeActivity);
    for (auto parentId : node->getChakraNode()->data_deps()) {
      auto parent = etFeeder->lookupNode(parentId);
      MemoryActivity readActivity = {
          MemoryActivityType::READ_END, parent, node, now};
      this->memoryActivities.push_back(readActivity);
    }
  }

  releasedNodes.emplace(node);
  if (node->getDataChildren().size() == 0 && this->trackMemoryActivities) {
    MemoryActivity freeActivity = {MemoryActivityType::FREE, node, node, now};
    this->memoryActivities.push_back(freeActivity);
  }
  const auto data_deps = node->getChakraNode()->data_deps();
  for (auto parentId : data_deps) {
    auto parent = etFeeder->lookupNode(parentId);
    bool depResolved = true;
    for (auto child : parent->getDataChildren()) {
      if (releasedNodes.count(child) == 0) {
        depResolved = false;
        break;
      }
    }
    if (depResolved) {
      size_t parent_size = parent->tensor_size(0ul);
      this->memoryContentSize -= parent_size;
      LoggerFactory::get_logger("local_memory_tracker")->critical("DEBUG: tensor_size={} memoryContentSize={}", parent->tensor_size(0ul), this->memoryContentSize);
      LoggerFactory::get_logger("local_memory_tracker")->flush();
      // if (this->memoryContentSize > 1e12)
      //   Sys::sys_panic("memory content size exceeded 1TB");
      this->memoryUsageTimeline.insert({now, this->memoryContentSize});
      if (this->trackMemoryActivities) {
        MemoryActivity freeActivity = {
            MemoryActivityType::FREE, parent, node, now};
        this->memoryActivities.push_back(freeActivity);
      }
    }
  }
}

void LocalMemoryTracker::report(std::string reportFolder) const {
  LoggerFactory::get_logger("local_memory_tracker")
      ->info(
          "sys[{}] peak memory usage: {}",
          this->workload->sys->id,
          this->peakMemoryUsage);
  // report memory activities
  if (this->trackMemoryActivities) {
    assert(std::filesystem::exists(reportFolder));
    json j;
    j["traceEvents"] = std::vector<json>();
    for (auto activity : this->memoryActivities) {
      auto node = activity.dataOwner;
      std::string ph, cat, type;
      switch (activity.type) {
        case READ_START:
          ph = "B";
          cat = "memory_activities";
          type = "read";
          break;
        case WRITE_START:
          ph = "B";
          cat = "memory_activities";
          type = "write";
          break;
        case ALLOC:
          ph = "B";
          cat = "memory_lifetime";
          type = "alloc";
          break;
        case READ_END:
          ph = "E";
          cat = "memory_activities";
          type = "read";
          break;
        case WRITE_END:
          ph = "E";
          cat = "memory_activities";
          type = "write";
          break;
        case FREE:
          ph = "E";
          cat = "memory_lifetime";
          type = "alloc";
          break;
        default:
          Sys::sys_panic("");
      }
      json event = {
          {"name", node->name()},
          {"cat", "memory activities"},
          {"ph", ph},
          {"ts", 1e-3 * activity.tick},
          {"pid", this->workload->sys->id},
          {"tid", node->id()},
          {"args",
           json{
               {"size", node->tensor_size(0ul)},
               {"type", type},
               {"caller", activity.caller->name()}}}};
      j["traceEvents"].push_back(event);
    }

    auto chromeTracePath = reportFolder + "/" + "chrome_trace." +
        std::to_string(this->workload->sys->id) + ".json";
    std::ofstream file(chromeTracePath);
    file << j.dump();
    file.close();
  }
}

LocalMemoryTracker::~LocalMemoryTracker() {
  this->memoryActivities.clear();
  this->memoryUsageTimeline.clear();
  this->releasedNodes.clear();
}
