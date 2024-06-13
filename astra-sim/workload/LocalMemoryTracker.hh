#ifndef __LOCAL_MEMORY_TRACKER_HH__
#define __LOCAL_MEMORY_TRACKER_HH__

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include "astra-sim/common/Common.hh"
#include "astra-sim/common/Logging.hh"
#include "astra-sim/system/Sys.hh"
#include "et_feeder/et_feeder.h"
#include "et_feeder/et_feeder_node.h"

namespace AstraSim {

class Workload;

class LocalMemoryTracker {
 public:
  enum MemoryActivityType {
    READ_START,
    READ_END,
    WRITE_START,
    WRITE_END,
    ALLOC,
    FREE
  };

  typedef struct {
    MemoryActivityType type;
    std::shared_ptr<Chakra::ETFeederNode> node;
    Tick tick;
  } MemoryActivity;

  LocalMemoryTracker(Workload* workload, bool trackMemoryActivities);

  void issueNode(
      Chakra::ETFeeder* etFeeder,
      std::shared_ptr<Chakra::ETFeederNode> node);

  void finishedNode(
      Chakra::ETFeeder* etFeeder,
      std::shared_ptr<Chakra::ETFeederNode> node);

  void report(std::string reportPath);

 private:
  Workload* workload;
  bool trackMemoryActivities;
  size_t memoryContentSize;
  size_t peakMemoryUsage;
  std::vector<MemoryActivity> memoryActivities;
  std::map<Tick, size_t> memoryUsageTimeline;
  std::unordered_set<std::shared_ptr<Chakra::ETFeederNode>> releasedNodes;
};

} // namespace AstraSim

#endif
