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
    static const Tick INVALID_TICK = UINT64_MAX;
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
          end_time(INVALID_TICK),
          type(type) {}
    OperatorStatistics()
        : node_id(UINT64_MAX),
          start_time(INVALID_TICK),
          end_time(INVALID_TICK),
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
  Statistics(Workload* workload);

  OperatorStatistics& get_operator_statistics(NodeId node_id);

  const OperatorStatistics& get_operator_statistics(NodeId node_id) const;

  const std::unordered_map<NodeId, OperatorStatistics>& get_operator_statistics()
      const;

  void record_start(
      std::shared_ptr<Chakra::ETFeederNode> node,
      Tick start_time);

  void record_end(std::shared_ptr<Chakra::ETFeederNode> node, Tick end_time);

  ~Statistics() {
    operator_statistics.clear();
  }

  void post_processing();

  void report(std::shared_ptr<spdlog::logger> logger) const;

  void report() const;

 private:
  void extract_type_time();
  Tick _calculateTotalRuntimeFromIntervals(
      const std::vector<std::pair<Tick, Tick>>& intervals) const;
  void extract_utilizations();

  std::unordered_map<OperatorStatistics::OperatorType, Tick> type_time;
  Tick wall_time;
  double compute_bound_percentage_;
  double average_compute_utilization_;
  double average_memory_utilization_;
  double average_operation_intensity_;
  Workload* workload;
  std::unordered_map<NodeId, OperatorStatistics> operator_statistics;
  std::multimap<Tick, NodeId> start_times;
  LocalMemoryTracker local_memory_tracker;
};

} // namespace AstraSim

#endif /*ASTRASIM_WORKLOAD_STATISTICS_HH*/
