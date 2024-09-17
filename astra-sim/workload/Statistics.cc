#include "astra-sim/workload/Statistics.hh"
#include <algorithm>
#include <cassert>
#include <map>
#include <unordered_map>
#include <vector>
#include "astra-sim/system/Sys.hh"
#include "astra-sim/workload/Workload.hh"

using namespace AstraSim;

Statistics::OperatorStatistics& Statistics::get_operator_statistics(
    NodeId node_id) {
  return operator_statistics.at(node_id);
}

const Statistics::OperatorStatistics& Statistics::get_operator_statistics(
    NodeId node_id) const {
  return operator_statistics.at(node_id);
}

void Statistics::record_start(
    std::shared_ptr<Chakra::ETFeederNode> node,
    Tick start_time) {
  const NodeId& node_id = node->id();
  const auto type = OperatorStatistics::get_operator_type(node);
  operator_statistics[node_id] = OperatorStatistics(node_id, start_time, type);
  start_times.insert({start_time, node_id});
  this->local_memory_tracker.issueNode(this->workload->et_feeder, node);
}

void Statistics::record_end(
    std::shared_ptr<Chakra::ETFeederNode> node,
    Tick end_time) {
  const NodeId& node_id = node->id();
  this->get_operator_statistics(node_id).end_time = end_time;
  this->local_memory_tracker.finishedNode(this->workload->et_feeder, node);
}

Statistics::OperatorStatistics::OperatorType Statistics::OperatorStatistics::
    get_operator_type(const std::shared_ptr<Chakra::ETFeederNode> node) {
  const auto& node_type = node->type();
  Statistics::OperatorStatistics::OperatorType stat_node_type;
  switch (node_type) {
    case ChakraNodeType::MEM_LOAD_NODE:
    case ChakraNodeType::MEM_STORE_NODE:
      stat_node_type = Statistics::OperatorStatistics::OperatorType::REMOTE_MEM;
      break;
    case ChakraNodeType::COMP_NODE:
      stat_node_type = node->is_cpu_op()
          ? Statistics::OperatorStatistics::OperatorType::CPU
          : Statistics::OperatorStatistics::OperatorType::GPU;
      break;
    case ChakraNodeType::COMM_COLL_NODE:
    case ChakraNodeType::COMM_SEND_NODE:
    case ChakraNodeType::COMM_RECV_NODE:
      stat_node_type = Statistics::OperatorStatistics::OperatorType::COMM;
      break;
    case ChakraNodeType::INVALID_NODE:
      stat_node_type = Statistics::OperatorStatistics::OperatorType::INVALID;
      break;
    default:
      LoggerFactory::get_logger("statistics")
          ->critical(
              "Invalid node_type, node.id={}, node.type={}",
              node->id(),
              static_cast<uint64_t>(node->type()));
      assert(false);
  }
  return stat_node_type;
}

std::unordered_map<Statistics::OperatorStatistics::OperatorType, Tick>
Statistics::extract_type_time() const {
  std::unordered_map<
      OperatorStatistics::OperatorType,
      std::vector<std::pair<Tick, Tick>>>
      interval_map;
  for (const auto& [node_id, stat] : operator_statistics) {
    interval_map[stat.type].push_back({stat.start_time, stat.end_time});
  }

  std::unordered_map<OperatorStatistics::OperatorType, Tick> type_total_time;
  for (const auto& [type, intervals] : interval_map) {
    type_total_time[type] = _calculateTotalRuntimeFromIntervals(intervals);
  }
  return type_total_time;
}

Tick Statistics::_calculateTotalRuntimeFromIntervals(
    const std::vector<std::pair<Tick, Tick>>& intervals) const {
  if (intervals.empty()) {
    return 0;
  }
  std::vector<std::pair<Tick, Tick>> sorted_intervals = intervals;
  sort(sorted_intervals.begin(), sorted_intervals.end());

  Tick total_runtime = 0;
  Tick merged_start = sorted_intervals[0].first;
  Tick merged_end = sorted_intervals[0].second;
  const auto& logger = LoggerFactory::get_logger("statistics");

  for (const auto& [start, end] : sorted_intervals) {
    if (start <= merged_end) {
      merged_end = std::max(merged_end, end);
    } else {
      total_runtime += merged_end - merged_start;
      merged_start = start;
      merged_end = end;
    }
  }
  total_runtime += merged_end - merged_start;
  return total_runtime;
}

void Statistics::report(std::shared_ptr<spdlog::logger> logger) const {
  Tick wall_time = 0;
  for (const auto& [node_id, stat] : operator_statistics) {
    if (stat.end_time == UINT64_MAX) {
      logger->critical(
          "Node {} did not finish, start_time={}", node_id, stat.start_time);
      exit(EXIT_FAILURE);
    } else {
      wall_time = std::max(wall_time, stat.end_time);
    }
  }
  const auto& sys_id = workload->sys->id;
  logger->info("sys[{}], Wall time: {}", sys_id, wall_time);
  for (const auto& [type, time] : extract_type_time()) {
    switch (type) {
      case OperatorStatistics::OperatorType::CPU:
        logger->info("sys[{}], CPU time: {}", sys_id, time);
        break;
      case OperatorStatistics::OperatorType::GPU:
        logger->info("sys[{}], GPU time: {}", sys_id, time);
        break;
      case OperatorStatistics::OperatorType::COMM:
        logger->info("sys[{}], Comm time: {}", sys_id, time);
        break;
      case OperatorStatistics::OperatorType::REMOTE_MEM:
        logger->info("sys[{}], Remote mem time: {}", sys_id, time);
        break;
      case OperatorStatistics::OperatorType::REPLAY:
        logger->info("sys[{}], Replay time: {}", sys_id, time);
        break;
      case OperatorStatistics::OperatorType::INVALID:
        logger->info("sys[{}], Invalid time: {}", sys_id, time);
        break;
    }
  }
  double compute_bound_percentage = this->compute_bound_percentage();
  logger->info(
      "sys[{}], Compute bound percentage: {:.6f}",
      sys_id,
      compute_bound_percentage);
  double average_compute_utilization = this->average_compute_utilization();
  logger->info(
      "sys[{}], Average compute utilization: {:.6f}",
      sys_id,
      average_compute_utilization);
  double average_memory_utilization = this->average_memory_utilization();
  logger->info(
      "sys[{}], Average memory utilization: {:.6f}",
      sys_id,
      average_memory_utilization);
  double average_operation_intensity = this->average_operation_intensity();
  logger->info(
      "sys[{}], Average operation intensity: {:.6f}",
      sys_id,
      average_operation_intensity);
  this->local_memory_tracker.report("");
}

void Statistics::report() const {
  report(LoggerFactory::get_logger("statistics"));
}

double Statistics::compute_bound_percentage() const {
  Tick compute_bound_time = 0;
  Tick total_time = 1ul;
  for (const auto& [node_id, stat] : operator_statistics) {
    if (stat.type != OperatorStatistics::OperatorType::CPU ||
        stat.type != OperatorStatistics::OperatorType::GPU) {
      if (!stat.is_memory_bound.has_value())
        continue;
      Tick duration = stat.end_time - stat.start_time;
      if (!stat.is_memory_bound.value())
        compute_bound_time += duration;
      total_time += duration;
    }
  }
  return static_cast<double>(compute_bound_time) / total_time;
}

double Statistics::average_compute_utilization() const {
  double total_compute_utilization = 0;
  Tick total_compute_time = 1ul;
  for (const auto& [node_id, stat] : operator_statistics) {
    if (stat.type == OperatorStatistics::OperatorType::CPU ||
        stat.type == OperatorStatistics::OperatorType::GPU) {
      if (!stat.compute_utilization.has_value())
        continue;
      Tick duration = stat.end_time - stat.start_time;
      total_compute_utilization += stat.compute_utilization.value() * duration;
      total_compute_time += duration;
    }
  }
  return total_compute_utilization / total_compute_time;
}

double Statistics::average_memory_utilization() const {
  double total_memory_utilization = 0;
  Tick total_compute_time = 1ul;
  for (const auto& [node_id, stat] : operator_statistics) {
    if (stat.type == OperatorStatistics::OperatorType::CPU ||
        stat.type == OperatorStatistics::OperatorType::GPU) {
      if (!stat.memory_utilization.has_value())
        continue;
      Tick duration = stat.end_time - stat.start_time;
      total_memory_utilization += stat.memory_utilization.value() * duration;
      total_compute_time += duration;
    }
  }
  return total_memory_utilization / total_compute_time;
}

double Statistics::average_operation_intensity() const {
  double total_operation_intensity = 0;
  Tick total_compute_time = 1ul;
  for (const auto& [node_id, stat] : operator_statistics) {
    if (stat.type == OperatorStatistics::OperatorType::CPU ||
        stat.type == OperatorStatistics::OperatorType::GPU) {
      if (!stat.operation_intensity.has_value())
        continue;
      Tick duration = stat.end_time - stat.start_time;
      total_operation_intensity += stat.operation_intensity.value() * duration;
      total_compute_time += duration;
    }
  }
  return total_operation_intensity / total_compute_time;
}
