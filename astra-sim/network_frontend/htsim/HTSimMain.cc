/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "HTSimNetworkApi.hh"
#include "HTSimSession.hh"
#include "astra-sim/common/Logging.hh"
#include "common/CmdLineParser.hh"
#include <astra-network-analytical/common/EventQueue.h>
#include <astra-network-analytical/common/NetworkParser.h>
#include <astra-network-analytical/congestion_unaware/Helper.h>
#include <remote_memory_backend/analytical/AnalyticalRemoteMemory.hh>

using namespace HTSim;

int main(int argc, char* argv[]) {
    // Parse command line arguments
    auto cmd_line_parser = CmdLineParser(argv[0]);
    cmd_line_parser.get_options().add_options()(
        "htsim-proto", "HTSim Network Protocol [tcp]",
        cxxopts::value<HTSimProto>()->default_value("tcp"));
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
    const auto logging_configuration =
        cmd_line_parser.get<std::string>("logging-configuration");
    const auto num_queues_per_dim =
        cmd_line_parser.get<int>("num-queues-per-dim");
    const auto comm_scale = cmd_line_parser.get<double>("comm-scale");
    const auto injection_scale = cmd_line_parser.get<double>("injection-scale");
    const auto rendezvous_protocol =
        cmd_line_parser.get<bool>("rendezvous-protocol");
    const auto proto = cmd_line_parser.get<HTSimProto>("htsim-proto");

    AstraSim::LoggerFactory::init(logging_configuration);

    // Generate topology
    const auto network_parser = NetworkParser(network_configuration);
    const auto topology = construct_topology(network_parser);

    // Get topology information
    const auto npus_count = topology->get_npus_count();
    const auto npus_count_per_dim = topology->get_npus_count_per_dim();
    const auto dims_count = topology->get_dims_count();

    // Set up Network API
    HTSimNetworkApi::set_topology(topology);
    auto completion_tracker = std::make_shared<CompletionTracker>(npus_count);
    HTSimNetworkApi::set_completion_tracker(completion_tracker);

    // Create ASTRA-sim related resources
    auto network_apis = std::vector<std::unique_ptr<HTSimNetworkApi>>();
    const auto memory_api =
        std::make_unique<Analytical::AnalyticalRemoteMemory>(
            remote_memory_configuration);
    auto systems = std::vector<Sys*>();

    auto queues_per_dim = std::vector<int>();
    for (auto i = 0; i < dims_count; i++) {
        queues_per_dim.push_back(num_queues_per_dim);
    }

    for (int i = 0; i < npus_count; i++) {
        // create network and system
        auto network_api = std::make_unique<HTSimNetworkApi>(i);
        auto* const system =
            new Sys(i, workload_configuration, comm_group_configuration,
                    system_configuration, memory_api.get(), network_api.get(),
                    npus_count_per_dim, queues_per_dim, injection_scale,
                    comm_scale, rendezvous_protocol);

        // push back network and system
        network_apis.push_back(std::move(network_api));
        systems.push_back(system);
    }

    // Get HTSim opts
    int htsim_argc = 0;
    char** htsim_argv = NULL;
    for (int i = 0; i < argc; i++) {
        if (std::string(argv[i]) == "--htsim_opts") {
            htsim_argc = argc - i;
            htsim_argv = argv + i;
        }
    }

    // Report HTSim opts
    for (int i = 0; i < htsim_argc; i++) {
        std::cout << htsim_argv[i] << " ";
    }
    std::cout << std::endl;

    // Initialize HTSim session
    HTSimNetworkApi::htsim_info.nodes = npus_count;
    // Choose protocol
    auto& ht = HTSimSession::init(&HTSimNetworkApi::htsim_info, htsim_argc,
                                  htsim_argv, proto);

    // Initiate simulation
    for (int i = 0; i < npus_count; i++) {
        systems[i]->workload->fire();
    }

    // run HTSim
    ht.run(&HTSimNetworkApi::htsim_info);

    // check if terminated properly
    if (!completion_tracker.get()->all_finished()) {
        std::cout << "Warning: Simulation timed out." << std::endl;
    }

    // terminate simulation
    AstraSim::LoggerFactory::shutdown();

    ht.finish();
    return 0;
}
