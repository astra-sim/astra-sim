#ifndef __LOCAL_MEMORY_TRACKER_HH__
#define __LOCAL_MEMORY_TRACKER_HH__

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include "astra-sim/common/Common.hh"
#include "astra-sim/common/Logging.hh"
#include "extern/graph_frontend/chakra/src/feeder_v2/et_feeder.h"
#include "extern/graph_frontend/chakra/src/feeder_v2/et_feeder_node.h"

namespace AstraSim {

class Workload;
class Statistics;

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
    NodeId dataOwner;
    NodeId caller;
    Tick tick;
  } MemoryActivity;

  LocalMemoryTracker(Statistics* statistics, Chakra::ETFeeder* et_feeder);

  void extractMemoryActivities();

  const std::unordered_map<NodeId, std::vector<MemoryActivity>>&
  getReadWriteActivities() const;

  const std::vector<MemoryActivity>& getAllocFreeActivities() const;

  void extractMemoryUsage();

  const std::map<Tick, uint64_t>& getMemoryUsage() const;

  void extractAvgPeakUsage();

  const uint64_t getAverageMemoryUsage() const;
  const uint64_t getPeakMemoryUsage() const;

 private:
  Statistics* statistics;
  Chakra::ETFeeder* et_feeder;
  std::unordered_map<NodeId, std::vector<MemoryActivity>> readWriteActivities;
  std::vector<MemoryActivity> allocFreeActivities;
  std::map<Tick, uint64_t> memory_usage;
  uint64_t average_memory_usage;
  uint64_t peak_memory_usage;
};

} // namespace AstraSim

#endif
