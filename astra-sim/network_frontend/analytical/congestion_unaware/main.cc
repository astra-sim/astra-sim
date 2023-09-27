/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include <astra-network-analytical/common/Common.hh>
#include <astra-network-analytical/common/event-queue/EventQueue.hh>
#include <astra-network-analytical/common/network-parser/NetworkParser.hh>
#include <astra-network-analytical/congestion_unaware/topology/Helper.hh>
#include "network_frontend/analytical/common/CmdLineParser.hh"

using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionUnaware;

int main(int argc, char* argv[]) {
  /// Parse command line arguments
  auto cmd_line_parser = CmdLineParser(argv[0]);
  cmd_line_parser.parse(argc, argv);

  /// Get command line arguments
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

  /// Instantiate event queue
  const auto event_queue = std::make_shared<EventQueue>();

  /// Generate topology
  const auto network_parser = NetworkParser(network_configuration);
  const auto topology = construct_topology(network_parser);

  /// Get topology information
  const auto npus_count = topology->get_npus_count();
  const auto npus_count_per_dim = topology->get_npus_count_per_dim();
  const auto dims_count = topology->get_dims_count();

  /// Set up Network API
//  NetworkApi::set_event_queue(event_queue);
//  NetworkApi::set_topology(topology);

  /// terminate
  std::cout << "Done!" << std::endl;
  return 0;
}
