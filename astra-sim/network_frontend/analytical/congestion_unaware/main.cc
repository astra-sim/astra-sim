/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include <astra-network-analytical/common/EventQueue.hh>
#include <astra-network-analytical/common/NetworkParser.hh>
#include <astra-network-analytical/congestion_unaware/Helper.hh>
#include <remote_memory_backend/analytical/AnalyticalRemoteMemory.hh>
#include "common/CmdLineParser.hh"
#include "congestion_unaware/CongestionUnawareNetworkApi.hh"

using namespace AstraSim;
using namespace Analytical;
using namespace AstraSimAnalytical;
using namespace AstraSimAnalyticalCongestionUnaware;
using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionUnaware;

int main(int argc, char* argv[]) {
  // Parse command line arguments
  auto cmd_line_parser = CmdLineParser(argv[0]);
  cmd_line_parser.parse(argc, argv);

  // Get command line arguments
  const auto workload_configuration =
      cmd_line_parser.get<std::string>("workload-configuration");
  const auto comm_group_configuration =
      cmd_line_parser.get<std::string>("comm-group-configuration");
  const auto system_configuration =
      cmd_line_parser.get<std::string>("system-configuration");
  const auto remote_memory_configuration =
      cmd_line_parser.get<std::string>("remote-memory-configuration");
  const auto network_configuration =
      cmd_line_parser.get<std::string>("network-configuration");
  const auto num_queues_per_dim =
      cmd_line_parser.get<int>("num-queues-per-dim");
  const auto comm_scale = cmd_line_parser.get<double>("comm-scale");
  const auto injection_scale = cmd_line_parser.get<double>("injection-scale");
  const auto rendezvous_protocol =
      cmd_line_parser.get<bool>("rendezvous-protocol");

  // Instantiate event queue
  const auto event_queue = std::make_shared<EventQueue>();

  // Generate topology
  const auto network_parser = NetworkParser(network_configuration);
  const auto topology = construct_topology(network_parser);

  // Get topology information
  const auto npus_count = topology->get_npus_count();
  const auto npus_count_per_dim = topology->get_npus_count_per_dim();
  const auto dims_count = topology->get_dims_count();

  // Set up Network API
  CongestionUnawareNetworkApi::set_event_queue(event_queue);
  CongestionUnawareNetworkApi::set_topology(topology);

  // Create ASTRA-sim related resources
  auto network_apis =
      std::vector<std::unique_ptr<CongestionUnawareNetworkApi>>();
  const auto memory_api =
      std::make_unique<AnalyticalRemoteMemory>(remote_memory_configuration);
  auto systems = std::vector<Sys*>();

  auto queues_per_dim = std::vector<int>();
  for (auto i = 0; i < dims_count; i++) {
    queues_per_dim.push_back(num_queues_per_dim);
  }

  for (int i = 0; i < npus_count; i++) {
    // create network and system
    auto network_api = std::make_unique<CongestionUnawareNetworkApi>(i);
    auto* const system = new Sys(
        i,
        workload_configuration,
        comm_group_configuration,
        system_configuration,
        memory_api.get(),
        network_api.get(),
        npus_count_per_dim,
        queues_per_dim,
        injection_scale,
        comm_scale,
        rendezvous_protocol);

    // push back network and system
    network_apis.push_back(std::move(network_api));
    systems.push_back(system);
  }

  // Initiate simulation
  for (int i = 0; i < npus_count; i++) {
    systems[i]->workload->fire();
  }

  // run simulation
  while (!event_queue->finished()) {
    event_queue->proceed();
  }

  for (int i = 0; i < npus_count; i++) {
    delete systems[i];
  }
  systems.clear();

  AstraSim::Logger::shutdown();

  // terminate simulation
  return 0;
}
