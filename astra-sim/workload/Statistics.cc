#include "astra-sim/workload/Statistics.hh"
#include "astra-sim/system/Sys.hh"
#include "astra-sim/workload/Workload.hh"
#include <algorithm>
#include <cassert>
#include <map>
#include <unordered_map>
#include <vector>

using namespace AstraSim;

Statistics::Statistics(Workload* workload) : workload(workload) {}

Statistics::OperatorStatistics& Statistics::get_operator_statistics(
    NodeId node_id) {
    return operator_statistics.at(node_id);
}

const Statistics::OperatorStatistics& Statistics::get_operator_statistics(
    NodeId node_id) const {
    return operator_statistics.at(node_id);
}

const std::unordered_map<NodeId, Statistics::OperatorStatistics>& Statistics::
    get_operator_statistics() const {
    return operator_statistics;
}

void Statistics::record_start(std::shared_ptr<Chakra::ETFeederNode> node,
                              Tick start_time) {
    const NodeId& node_id = node->id();
    const auto type = OperatorStatistics::get_operator_type(node);
    operator_statistics[node_id] =
        OperatorStatistics(node_id, start_time, type);
    start_times.insert({start_time, node_id});
}

void Statistics::record_end(std::shared_ptr<Chakra::ETFeederNode> node,
                            Tick end_time) {
    const NodeId& node_id = node->id();
    this->get_operator_statistics(node_id).end_time = end_time;
}

Statistics::OperatorStatistics::OperatorType Statistics::OperatorStatistics::
    get_operator_type(const std::shared_ptr<Chakra::ETFeederNode> node) {
    const auto& node_type = node->type();
    Statistics::OperatorStatistics::OperatorType stat_node_type;
    switch (node_type) {
    case ChakraNodeType::MEM_LOAD_NODE:
    case ChakraNodeType::MEM_STORE_NODE:
        stat_node_type =
            Statistics::OperatorStatistics::OperatorType::REMOTE_MEM;
        break;
    case ChakraNodeType::COMP_NODE:
        stat_node_type =
            node->is_cpu_op()
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
            ->critical("Invalid node_type, node.id={}, node.type={}",
                       node->id(), static_cast<uint64_t>(node->type()));
        assert(false);
    }
    return stat_node_type;
}

void Statistics::extract_type_time() {
    std::unordered_map<OperatorStatistics::OperatorType,
                       std::vector<std::pair<Tick, Tick>>>
        interval_map;
    for (const auto& [node_id, stat] : operator_statistics) {
        interval_map[stat.type].push_back({stat.start_time, stat.end_time});
    }

    this->type_time.clear();
    for (const auto& [type, intervals] : interval_map) {
        this->type_time[type] = _calculateTotalRuntimeFromIntervals(intervals);
    }
}

void Statistics::extract_comp_comm_overlap() {
    bool has_comp = false;
    bool has_comm = false;
    for (const auto& [type, time] : this->type_time) {
        switch (type) {
        case OperatorStatistics::OperatorType::GPU:
            has_comp = true;
            break;
        case OperatorStatistics::OperatorType::COMM:
            has_comm = true;
            break;
        default:
            throw std::runtime_error(
                "Only GPU and COMM types are supported for overlap extraction");
        }
    }
    if (!has_comp || !has_comm) {
        this->comp_comm_overlap = 0;
        return;
    }
    Tick overlap = 0;
    overlap = this->type_time.at(OperatorStatistics::OperatorType::GPU) +
              this->type_time.at(OperatorStatistics::OperatorType::COMM) -
              this->wall_time;
    this->comp_comm_overlap = overlap;
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
    const auto& sys_id = workload->sys->id;
    logger->info("sys[{}], Wall time: {}", sys_id, this->wall_time);
    for (const auto& [type, time] : this->type_time) {
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
    logger->info("sys[{}], Total compute-communication overlap: {}", sys_id,
                 this->comp_comm_overlap);

    // Report network bandwidth for communication operations
    // logger->info("sys[{}], Network bandwidth details:", sys_id);

    // double total_bandwidth = 0.0;
    // size_t num_comm_ops = 0;

    // for (const auto& [node_id, stat] : operator_statistics) {
    //   if (stat.type == OperatorStatistics::OperatorType::COMM &&
    //       stat.network_bandwidth.has_value() &&
    //       stat.comm_size.has_value()) {
    //     // Convert from bytes/ns to GB/s (1 GB/s = 1 byte/ns)
    //     double bandwidth_gbps = stat.network_bandwidth.value();
    //     total_bandwidth += bandwidth_gbps;
    //     num_comm_ops++;

    //     logger->info("  Node {}: Size: {} bytes, Duration: {} ns, Bandwidth:
    //     {:.3f} GB/s",
    //                 node_id,
    //                 stat.comm_size.value(),
    //                 stat.end_time - stat.start_time,
    //                 bandwidth_gbps);
    //   }
    // }

    // if (num_comm_ops > 0) {
    //   double avg_bandwidth = total_bandwidth / num_comm_ops;
    //   logger->info("sys[{}], Average network bandwidth: {:.3f} GB/s across {}
    //   communication operations",
    //               sys_id, avg_bandwidth, num_comm_ops);
    // }

    // only report utilization statistics when roofline is enabled
    if (workload->sys->roofline_enabled) {
        logger->info("sys[{}], Compute bound percentage: {:.3f}%", sys_id,
                     this->compute_bound_percentage_ * 100);
        logger->info("sys[{}], Average compute utilization: {:.3f}%", sys_id,
                     this->average_compute_utilization_ * 100);
        logger->info("sys[{}], Average memory utilization: {:.3f}%", sys_id,
                     this->average_memory_utilization_ * 100);
        logger->info("sys[{}], Average operation intensity: {:.3f}", sys_id,
                     this->average_operation_intensity_);
    }
}

void Statistics::report() const {
    report(LoggerFactory::get_logger("statistics"));
}

void Statistics::extract_utilizations() {
    Tick total_compute_bound_time = 0;
    double total_compute_utilization = 0;
    double total_memory_utilization = 0;
    double total_operation_intensity = 0;
    Tick total_compute_time = 1ul;  // To avoid division by zero

    for (const auto& [node_id, stat] : operator_statistics) {
        if (stat.type == OperatorStatistics::OperatorType::CPU ||
            stat.type == OperatorStatistics::OperatorType::GPU) {
            Tick duration = stat.end_time - stat.start_time;

            if (stat.is_memory_bound.has_value() &&
                !stat.is_memory_bound.value()) {
                total_compute_bound_time += duration;
            }

            if (stat.compute_utilization.has_value()) {
                total_compute_utilization +=
                    stat.compute_utilization.value() * duration;
            }

            if (stat.memory_utilization.has_value()) {
                total_memory_utilization +=
                    stat.memory_utilization.value() * duration;
            }

            if (stat.operation_intensity.has_value()) {
                total_operation_intensity +=
                    stat.operation_intensity.value() * duration;
            }

            total_compute_time += duration;
        }
    }

    this->compute_bound_percentage_ =
        static_cast<double>(total_compute_bound_time) / total_compute_time;
    this->average_compute_utilization_ =
        total_compute_utilization / total_compute_time;
    this->average_memory_utilization_ =
        total_memory_utilization / total_compute_time;
    this->average_operation_intensity_ =
        total_operation_intensity / total_compute_time;
}

void Statistics::post_processing() {
    const auto& logger = LoggerFactory::get_logger("statistics");
    logger->info("sys[{}]. Post statistics processing start.",
                 this->workload->sys->id);

    this->wall_time = 0;
    for (const auto& [node_id, stat] : operator_statistics) {
        if (stat.end_time == Statistics::OperatorStatistics::INVALID_TICK) {
            logger->critical("Node {} did not finish, start_time={}", node_id,
                             stat.start_time);
            exit(EXIT_FAILURE);
        } else {
            this->wall_time = std::max(this->wall_time, stat.end_time);
        }
    }
    extract_type_time();
    if (workload->sys->roofline_enabled) {
        extract_utilizations();
    }
    extract_comp_comm_overlap();

    logger->info("sys[{}]. Post statistics processing end.",
                 this->workload->sys->id);
}
