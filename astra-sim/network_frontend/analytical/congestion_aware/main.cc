/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include <iostream> // Divya: to port congestion backend to Chakra
#include <vector>
#include "AstraCongestionApi.hh"
#include "astra-sim/system/Sys.hh"
// #include "astra-sim/system/memory/SimpleMemory.hh"
#include <json/json.hpp>
#include <congestion_aware/event-queue/EventQueue.hh>
#include "extern/memory_backend/analytical/AnalyticalMemory.hh" // Divya: to port congestion backend to latest Chakra
#include <congestion_aware/helper/CommandLineOptParser.hh>
#include <congestion_aware/helper/NetworkConfigParser.hh>
#include <congestion_aware/topology/FullyConnected.hh>
#include <congestion_aware/topology/Ring.hh>
#include <congestion_aware/topology/Switch.hh>
#include <congestion_aware/topology/Topology.hh>

using namespace Congestion;
using json = nlohmann::json;

int get_num_npus(std::vector<int> units_counts) {
  int num_npus = 1;
  for (auto units_count : units_counts) {
    num_npus *= units_count;
  }
  return num_npus;
}

int main(int argc, char* argv[]) {
  /// Create command line parser
  auto command_line_parser = CommandLineOptParser();

  // add command line options
  // file path
  command_line_parser.define_option<std::string>(
      "network-configuration", "Network Configuration File Path");
  command_line_parser.define_option<std::string>(
      "system-configuration", "system Configuration File Path");
  command_line_parser.define_option<std::string>(
      "workload-configuration", "Workload Configuration File Path");
  command_line_parser.define_option<std::string>(
      "memory-configuration", "Memory configuration File Path");
  command_line_parser.define_option<std::string>(
      "comm-group-configuration", "Communicator group configuration file");

  // run-related names and paths
  command_line_parser.define_option<std::string>("run-name", "Run Name");
  command_line_parser.define_option<std::string>(
      "path", "Result directory path");
  command_line_parser.define_option<int>("total-stat-rows", "Total stat rows");
  command_line_parser.define_option<int>("stat-row", "Current stat row");

  // scale configuration
  command_line_parser.define_option<float>(
      "comm-scale", "Communication scale", 1.0);
  command_line_parser.define_option<float>(
      "compute-scale", "Communication scale", 1.0);
  command_line_parser.define_option<float>(
      "injection-scale", "Communication scale", 1.0);
  command_line_parser.define_option<int>(
      "num-passes", "Number of run passes", 1);
  command_line_parser.define_option<bool>(
      "rendezvous-protocol", "Rendezvous Protocol", false);

  // topology-related options that could be overridden
  command_line_parser.define_option<int>(
      "npus-count", "Npus count", OptType::Multi);
  command_line_parser.define_option<double>(
      "bandwidth", "Bandwidth of each dim", OptType::Multi);
  command_line_parser.define_option<double>(
      "latency", "Latency of each dim", OptType::Multi);

  // parse command line
  command_line_parser.parse(argc, argv);

  // print help message
  try {
    command_line_parser.print_help_message_if_required();
  } catch (const HelpMessagePrinted&) {
    return 1;
  }

  // get required command line option values
  auto network_file_path = std::string();
  auto system_file_path = std::string();
  auto workload_file_path = std::string();
  auto memory_file_path = std::string();
  auto comm_group_configuration = "empty";

  auto run_name = std::string();
  auto path = std::string();

  auto comm_scale = 0.0f;
  auto compute_scale = 0.0f;
  auto injection_scale = 0.0f;
  auto num_passes = -1;
  auto total_stat_rows = -1;
  auto stat_row = -1;
  auto rendezvous_protocol = false;

  // first, parse network file
  // parse network file path
  try {
    network_file_path =
        command_line_parser.get_value<std::string>("network-configuration");
  } catch (const OptNotSpecifiedError& e) {
    std::cout << e.what() << std::endl;
    return 2;
  }

  // parse network configuration file
  auto network_config_parser = NetworkConfigParser();
  try {
    network_config_parser.parse(network_file_path);
  } catch (const YAML::BadFile& e) {
    std::cout << e.what() << std::endl;
    return 3;
  } catch (const YAML::ParserException& e) {
    std::cout << e.what() << std::endl;
    return 4;
  }

  // get network configuration values
  auto topology_name = std::string();
  auto npus_count_dim = std::vector<int>();
  auto bandwidth = std::vector<double>();
  auto latency = std::vector<double>();
  try {
    topology_name = network_config_parser.get_value<std::string>("topology");
    npus_count_dim =
        network_config_parser.get_value<std::vector<int>>("npus_count");
    bandwidth =
        network_config_parser.get_value<std::vector<double>>("bandwidth");
    latency = network_config_parser.get_value<std::vector<double>>("latency");
  } catch (const OptNotSpecifiedError& e) {
    std::cout << e.what() << std::endl;
    return 5;
  }

  // parse command line options
  try {
    system_file_path =
        command_line_parser.get_value<std::string>("system-configuration");
    workload_file_path =
        command_line_parser.get_value<std::string>("workload-configuration");
    memory_file_path =
        command_line_parser.get_value<std::string>("memory-configuration");

    // run_name = command_line_parser.get_value<std::string>("run-name");
    // path = command_line_parser.get_value<std::string>("path");

    comm_scale = command_line_parser.get_value<float>("comm-scale");
    compute_scale = command_line_parser.get_value<float>("compute-scale");
    injection_scale = command_line_parser.get_value<float>("injection-scale");
    // num_passes = command_line_parser.get_value<int>("num-passes");
    // total_stat_rows = command_line_parser.get_value<int>("total-stat-rows");
    // stat_row = command_line_parser.get_value<int>("stat-row");
    rendezvous_protocol =
        command_line_parser.get_value<bool>("rendezvous-protocol");
  } catch (const OptNotSpecifiedError& e) {
    std::cout << e.what() << std::endl;
    return 2;
  }

  // override topology-related options if specified
  auto npus_count_opt =
      command_line_parser.get_optional_value<std::vector<int>>("npus-count");
  auto bandwidth_opt =
      command_line_parser.get_optional_value<std::vector<double>>("bandwidth");
  auto latency_opt =
      command_line_parser.get_optional_value<std::vector<double>>("latency");

  if (npus_count_opt.has_value()) {
    npus_count_dim = npus_count_opt.value();
  }
  if (bandwidth_opt.has_value()) {
    bandwidth = bandwidth_opt.value();
  }
  if (latency_opt.has_value()) {
    latency = latency_opt.value();
  }

  /// Instantiate shared resources
  auto event_queue = std::make_shared<EventQueue>();

  /// Setup topology
  Topology::link_event_queue(event_queue);

  // create topology
  std::shared_ptr<Topology> topology;
  auto npus_count = -1;
  if (topology_name == "Ring") {
    npus_count = npus_count_dim[0];
    topology =
        std::make_shared<Ring>(npus_count, bandwidth[0], latency[0], true);
  } else if (topology_name == "Switch") {
    npus_count = npus_count_dim[0];
    topology = std::make_shared<Switch>(npus_count, bandwidth[0], latency[0]);
  } else if (topology_name == "FullyConnected") {
    npus_count = npus_count_dim[0];
    topology =
        std::make_shared<FullyConnected>(npus_count, bandwidth[0], latency[0]);
  } else {
    std::cout << "Unknown topology name: " << topology_name << std::endl;
    return 6;
  }

  /// Setup Common resources
  Link::link_event_queue(event_queue);
  AstraCongestionApi::link_event_queue(event_queue);
  AstraCongestionApi::link_topology(topology);

  /// Create ASTRA-sim related resources
  std::shared_ptr<Congestion::AstraCongestionApi> congestion_apis[npus_count];
  // std::shared_ptr<AstraSim::SimpleMemory> memories[npus_count];
  std::unique_ptr<Analytical::AnalyticalMemory> mem =
      std::make_unique<Analytical::AnalyticalMemory>(memory_file_path);

  AstraSim::Sys* systems[npus_count];

  auto queues_per_dim = std::vector<int>();
  for (int i = 0; i < npus_count_dim.size(); i++) {
    queues_per_dim.push_back(1);
  }

  for (int i = 0; i < npus_count; i++) {
    congestion_apis[i] = std::make_shared<AstraCongestionApi>(i);

    // memories[i] = std::make_shared<AstraSim::SimpleMemory>(
    // memories[i] = std::make_unique<AstraSim::SimpleMemory>(
    //     (AstraSim::AstraNetworkAPI*)(congestion_apis[i].get()),
    //     1,
    //     500000,
    //     12.5);

    /*
    systems[i] = new AstraSim::Sys(
        congestion_apis[i].get(),
        memories[i].get(),
        i,
        num_passes,
        npus_count_dim,
        queues_per_dim,
        system_file_path,
        workload_file_path,
        comm_scale,
        compute_scale,
        injection_scale,
        total_stat_rows,
        stat_row,
        path,
        run_name,
        true,
        rendezvous_protocol);
    */

    systems[i] = new AstraSim::Sys(
        i,
        workload_file_path,
        comm_group_configuration,
        system_file_path,
        mem.get(),
        // memories[i].get(),
        congestion_apis[i].get(),
        npus_count_dim,
        queues_per_dim,
        injection_scale,
        comm_scale,
        rendezvous_protocol);
  }

  /// Run ASTRA-sim simulation
  for (int i = 0; i < npus_count; i++) {
    systems[i]->workload->fire();
  }

  while (!event_queue->finished()) {
    event_queue->proceed();
  }

  /// terminate simulation
  return 0;
}
