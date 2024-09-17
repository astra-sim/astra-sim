#ifndef ASTRASIM_WORKLOAD_STATISTICS_HH
#define ASTRASIM_WORKLOAD_STATISTICS_HH

#include <map>
#include <optional>
#include <unordered_map>
#include "astra-sim/common/Common.hh"
#include "astra-sim/common/Logging.hh"
#include "astra-sim/workload/LocalMemoryTracker.hh"
#include "extern/graph_frontend/chakra/src/feeder_v2/et_feeder.h"
typedef ChakraProtoMsg::NodeType ChakraNodeType;

typedef uint64_t NodeId;

namespace AstraSim {
class Workload;
class LocalMemoryTracker;
class Statistics {
 public:
  class OperatorStatistics {
   public:
    enum class OperatorType { CPU, GPU, COMM, REMOTE_MEM, REPLAY, INVALID };
    static OperatorType get_operator_type(
        const std::shared_ptr<Chakra::ETFeederNode> node);
    OperatorStatistics(
        NodeId node_id,
        Tick start_time,
        Tick end_time,
        OperatorType type)
        : node_id(node_id),
          start_time(start_time),
          end_time(end_time),
          type(type) {}
    OperatorStatistics(NodeId node_id, Tick start_time, OperatorType type)
        : node_id(node_id),
          start_time(start_time),
          end_time(UINT64_MAX),
          type(type) {}
    OperatorStatistics()
        : node_id(UINT64_MAX),
          start_time(UINT64_MAX),
          end_time(UINT64_MAX),
          type(OperatorType::INVALID) {}

    NodeId node_id;
    Tick start_time;
    Tick end_time;
    OperatorType type;

    // compute node
    std::optional<double> memory_utilization;
    std::optional<double> compute_utilization;
    std::optional<double> operation_intensity;
    std::optional<bool> is_memory_bound;

    // communication node

    // remote memory node

    // replay node
  };

 public:
  Statistics(Workload* workload, bool track_memory_activities)
      : workload(workload),
        local_memory_tracker(workload, track_memory_activities) {}

  OperatorStatistics& get_operator_statistics(NodeId node_id);

  const OperatorStatistics& get_operator_statistics(NodeId node_id) const;

  void record_start(
      std::shared_ptr<Chakra::ETFeederNode> node,
      Tick start_time);

  void record_end(std::shared_ptr<Chakra::ETFeederNode> node, Tick end_time);

  ~Statistics() {
    operator_statistics.clear();
  }

  void report(std::shared_ptr<spdlog::logger> logger) const;

  void report() const;

 private:
  std::unordered_map<Statistics::OperatorStatistics::OperatorType, Tick>
  extract_type_time() const;
  Tick _calculateTotalRuntimeFromIntervals(
      const std::vector<std::pair<Tick, Tick>>& intervals) const;
  double compute_bound_percentage() const;
  double average_compute_utilization() const;
  double average_memory_utilization() const;
  double average_operation_intensity() const;
  Workload* workload;
  std::unordered_map<NodeId, OperatorStatistics> operator_statistics;
  std::multimap<Tick, NodeId> start_times;
  LocalMemoryTracker local_memory_tracker;
};

} // namespace AstraSim

#endif /*ASTRASIM_WORKLOAD_STATISTICS_HH*/
