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
}

void Statistics::record_end(NodeId node_id, Tick end_time) {
  this->get_operator_statistics(node_id).end_time = end_time;
}
