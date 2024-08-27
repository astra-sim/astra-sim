#include "astra-sim/workload/Statistics.hh"

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
    NodeId node_id,
    Tick start_time,
    OperatorStatistics::OperatorType type) {
  operator_statistics[node_id] = OperatorStatistics(node_id, start_time, type);
  start_times.insert({start_time, node_id});
}

void Statistics::record_end(NodeId node_id, Tick end_time) {
  this->get_operator_statistics(node_id).end_time = end_time;
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

void Statistics::extract_type_time() const {
  std::map<Tick, uint64_t> type_timeline;
}
