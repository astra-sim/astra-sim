#ifndef ASTRASIM_WORKLOAD_STATISTICS_HH
#define ASTRASIM_WORKLOAD_STATISTICS_HH

#include <optional>
#include <unordered_map>
#include "astra-sim/common/Common.hh"

typedef uint64_t NodeId;

namespace AstraSim {
class Statistics {
 public:
  class OperatorStatistics {
   public:
    enum class OperatorType { CPU, GPU, COMM, REMOTE_MEM, REPLAY, INVALID };
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
  Statistics() {}

  OperatorStatistics& get_operator_statistics(NodeId node_id);

  const OperatorStatistics& get_operator_statistics(NodeId node_id) const;

  void record_start(
      NodeId node_id,
      Tick start_time,
      OperatorStatistics::OperatorType type);

  void record_end(NodeId node_id, Tick end_time);

  ~Statistics() {
    operator_statistics.clear();
  }

 private:
  std::unordered_map<NodeId, OperatorStatistics> operator_statistics;
};

} // namespace AstraSim

#endif /*ASTRASIM_WORKLOAD_STATISTICS_HH*/
