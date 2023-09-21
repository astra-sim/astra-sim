/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include <iostream>

#include "AnalyticalNetwork.hh"
#include <json/json.hpp>
#include "astra-sim/system/Sys.hh"
#include "event-queue/EventQueue.hh"
#include "event-queue/EventQueueEntry.hh"
#include "extern/memory_backend/analytical/AnalyticalMemory.hh"
#include "helper/NetworkConfigParser.hh"
#include "helper/cxxopts.hpp"
#include "topology/HierarchicalTopology.hh"
#include "topology/HierarchicalTopologyConfig.hh"

using namespace std;
using json = nlohmann::json;

int get_num_npus(vector<int> units_counts) {
  int num_npus = 1;
  for (auto units_count : units_counts) {
    num_npus *= units_count;
  }
  return num_npus;
}

void get_comm_topology(
    Analytical::Topology::TopologyConfigs& topology_configs,
    string network_configuration,
    int num_queues_per_dim,
    shared_ptr<Analytical::Topology>& topology,
    int& dimensions_count,
    vector<int>& units_counts,
    vector<int>& physical_dims,
    vector<int>& queues_per_dim) {
  auto network_parser = Analytical::NetworkConfigParser(network_configuration);
  string topology_name = network_parser.get<string>("topology-name");
  dimensions_count = network_parser.get<int>("dimensions-count");
  units_counts = network_parser.get<vector<int>>("units-count");
  auto link_latencies = network_parser.get<vector<double>>("link-latency");
  auto link_bandwidths = network_parser.get<vector<double>>("link-bandwidth");
  auto nic_latencies = network_parser.get<vector<double>>("nic-latency");
  auto router_latencies = network_parser.get<vector<double>>("router-latency");
  auto hbm_latencies = network_parser.get<vector<double>>("hbm-latency");
  auto hbm_bandwidths = network_parser.get<vector<double>>("hbm-bandwidth");
  auto hbm_scales = network_parser.get<vector<double>>("hbm-scale");

  for (int dim = 0; dim < dimensions_count; dim++) {
    physical_dims.emplace_back(units_counts[dim]);
  }

  queues_per_dim = vector<int>(dimensions_count, num_queues_per_dim);

  // topology configuration for each dimension
  for (int i = 0; i < dimensions_count; i++) {
    auto link_bandwidth_b_ns = (double)link_bandwidths[i] * (1 << 30) /
        (1'000'000'000); // link bandwidth in B/ns

    topology_configs.emplace_back(
        units_counts[i], // NPUs count
        link_latencies[i], // link latency (ns)
        link_bandwidth_b_ns, // link bandwidth (B/ns)
        nic_latencies[i], // nic latency (ns)
        router_latencies[i], // router latency (ns)
        hbm_latencies[i], // memory latency (ns),
        hbm_bandwidths[i], // memory bandwidth (GB/s) = (B/ns)
        hbm_scales[i] // memory scaling factor
    );
  }

  if (topology_name == "Hierarchical") {
    auto topologies_per_dim = network_parser.parseHierarchicalTopologyList();
    auto dimension_types = network_parser.parseHierarchicalDimensionType();
    auto links_count_per_dim = network_parser.parseLinksCountPerDim();

    auto hierarchy_config = Analytical::HierarchicalTopologyConfig(
        dimensions_count,
        topologies_per_dim,
        dimension_types,
        links_count_per_dim,
        link_bandwidths);

    topology = make_shared<Analytical::HierarchicalTopology>(
        topology_configs, hierarchy_config);
  } else {
    cout << "[Analytical, main] Topology not defined: " << topology_name
         << endl;
  }
}

int main(int argc, char* argv[]) {
  try {
    //-------------------------------------------------------------------------
    // Parse configurations
    //-------------------------------------------------------------------------
    cxxopts::Options options(argv[0], "ASTRA-sim");

    options.set_width(70).allow_unrecognised_options().add_options()(
        "workload-configuration",
        "Workload configuration file",
        cxxopts::value<string>())(
        "comm-group-configuration",
        "Communicator group configuration file",
        cxxopts::value<string>()->default_value("empty"))(
        "system-configuration",
        "System configuration file",
        cxxopts::value<string>())(
        "memory-configuration",
        "Memory configuration file",
        cxxopts::value<string>())(
        "network-configuration",
        "Network configuration file",
        cxxopts::value<string>())(
        "num-queues-per-dim",
        "Number of queues per each dimension",
        cxxopts::value<int>()->default_value("1"))(
        "compute-scale",
        "Compute scale",
        cxxopts::value<double>()->default_value("1"))(
        "comm-scale",
        "Communication scale",
        cxxopts::value<double>()->default_value("1"))(
        "injection-scale",
        "Injection scale",
        cxxopts::value<double>()->default_value("1"))(
        "rendezvous-protocol",
        "Whether to enable rendezvous protocol",
        cxxopts::value<bool>()->default_value("false"));

    auto result = options.parse(argc, argv);
    string workload_configuration =
        result["workload-configuration"].as<string>();
    string comm_group_configuration =
        result["comm-group-configuration"].as<string>();
    string system_configuration = result["system-configuration"].as<string>();
    string memory_configuration = result["memory-configuration"].as<string>();
    string network_configuration = result["network-configuration"].as<string>();
    int num_queues_per_dim = result["num-queues-per-dim"].as<int>();
    double comm_scale = result["comm-scale"].as<double>();
    double injection_scale = result["injection-scale"].as<double>();
    bool rendezvous_protocol = result["rendezvous-protocol"].as<bool>();
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    // Event queue
    //-------------------------------------------------------------------------
    auto event_queue = make_shared<Analytical::EventQueue>();
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    // System
    //-------------------------------------------------------------------------
    shared_ptr<Analytical::Topology> comm_topology;
    int dimensions_count;
    auto units_counts = vector<int>();
    auto physical_dims =
        vector<int>(); // number of nodes for each system layer dimension
    auto queues_per_dim = vector<int>();
    auto topology_configs = Analytical::Topology::TopologyConfigs();
    get_comm_topology(
        topology_configs,
        network_configuration,
        num_queues_per_dim,
        comm_topology, // ref
        dimensions_count, // ref
        units_counts, // ref
        physical_dims, // ref
        queues_per_dim); // ref
    int num_npus = get_num_npus(units_counts);
    unique_ptr<Analytical::AnalyticalNetwork> comm_networks[num_npus];
    Analytical::AnalyticalNetwork::set_event_queue(event_queue);
    Analytical::AnalyticalNetwork::set_topology(comm_topology);

    AstraSim::Sys* systems[num_npus];
    std::unique_ptr<Analytical::AnalyticalMemory> mem =
        std::make_unique<Analytical::AnalyticalMemory>(memory_configuration);

    for (int npu_id = 0; npu_id < num_npus; npu_id++) {
      comm_networks[npu_id] =
          make_unique<Analytical::AnalyticalNetwork>(npu_id, dimensions_count);

      systems[npu_id] = new AstraSim::Sys(
          npu_id,
          workload_configuration,
          comm_group_configuration,
          system_configuration,
          mem.get(),
          comm_networks[npu_id].get(),
          physical_dims,
          queues_per_dim,
          injection_scale,
          comm_scale,
          rendezvous_protocol);
    }
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    // Run
    //-------------------------------------------------------------------------
    for (int i = 0; i < num_npus; i++) {
      systems[i]->workload->fire();
    }

    while (!event_queue->empty()) {
      event_queue->proceed();
    }
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    // Clean-up
    //-------------------------------------------------------------------------

    // System class automatically deletes itself, so no need to free systems[i]
    // here. Invoking `free systems[i]` here will trigger segfault (by trying to
    // delete already deleted memory space)

  } catch (const cxxopts::OptionException& e) {
    cout << "error parsing options: " << e.what() << endl;
    return 1;
  }

  return 0;
}
