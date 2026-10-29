// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-memory-analytical/AnalyticalRemoteMemory.hh>
#include <astra-network-analytical/common/EventQueue.h>
#include <astra-network-analytical/common/NetworkParser.h>
#include <astra-network-analytical/congestion_aware/Helper.h>
#include <astra-sim/system/gpu/GpuSys.hh>
#include <astra-sim/system/simple/SimpleSys.hh>
#include <astra-sim/workload/Workload.hh>
#include <common/CmdLineParser.hh>
#include <congestion_aware/CongestionAwareNetworkApi.hh>

using namespace AstraSim;
using namespace Analytical;
using namespace AstraSimAnalytical;
using namespace AstraSimAnalyticalCongestionAware;
using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionAware;

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
    const auto comm_size = cmd_line_parser.get<uint64_t>("comm-size");
    const auto custom_comm_plan_map =
        cmd_line_parser.get<std::string>("custom-comm-plan-map");
    const auto stat_file_path = cmd_line_parser.get<std::string>("stat-file");
    const auto detailed_stats =
        cmd_line_parser.get<bool>("detailed-network-stats");
    const auto periodic_stat_period =
        cmd_line_parser.get<int>("periodic-stats-period");

    // Instantiate event queue
    const auto event_queue = std::make_shared<EventQueue>();
    Topology::set_event_queue(event_queue);

    // Generate topology
    const auto network_parser = NetworkParser(network_configuration);
    const auto topology = construct_topology(network_parser);

    // Get topology information
    const auto npus_count = topology->get_npus_count();
    const auto npus_count_per_dim = topology->get_npus_count_per_dim();
    const auto dims_count = topology->get_dims_count();

    // Set up Network API
    CongestionAwareNetworkApi::set_event_queue(event_queue);
    CongestionAwareNetworkApi::set_topology(topology);

    // Create ASTRA-sim related resources
    auto network_apis =
        std::vector<std::unique_ptr<CongestionAwareNetworkApi>>();
    const auto memory_api =
        std::make_unique<AnalyticalRemoteMemory>(remote_memory_configuration);
    auto systems = std::vector<Sys*>();

    if (stat_file_path.length() > 0) {
        g_stat_collection.init(stat_file_path);
    }

    auto queues_per_dim = std::vector<int>();
    for (auto i = 0; i < dims_count; i++) {
        queues_per_dim.push_back(num_queues_per_dim);
    }

    // stats
    std::vector<std::tuple<int, int, int>> histos_nbins_min_max = {
        {100, 0, 2000000},  // noc_mem_req 100 bins in range [0, 2us]
        {100, 0, 2000000},  // noc_mem_resp 100 bins in range [0, 2us]
        {100, 0, 4000000},  // remote_st 100 bins in range [0, 4us]
        {100, 0, 4000000}   // remote_ack 100 bins in range [0, 4us]
    };
    timespec_t clock_period(AstraSim::PS, 1);

    // Builds the Sys flavor named by "system-type" (absent means Simple)
    const auto system_type = Sys::readSystemType(system_configuration);
    const auto create_system = [&](int id,
                                   AstraNetworkAPI* network_api) -> Sys* {
        if (system_type == "GPU") {
            return new GpuSys(id, workload_configuration,
                              comm_group_configuration, system_configuration,
                              memory_api.get(), network_api, clock_period,
                              comm_size, custom_comm_plan_map);
        }
        if (system_type == "Simple" || system_type.empty()) {
            return new SimpleSys(
                id, workload_configuration, comm_group_configuration,
                system_configuration, memory_api.get(), network_api,
                npus_count_per_dim, queues_per_dim, injection_scale, comm_scale,
                rendezvous_protocol, comm_size, custom_comm_plan_map);
        }
        Sys::sysPanic("Unknown system-type: " + system_type);
        return nullptr;
    };

    for (int i = 0; i < npus_count; i++) {
        // create network and system
        auto network_api = std::make_unique<CongestionAwareNetworkApi>(i);
        auto* const system = create_system(i, network_api.get());

        network_api->init_stats(histos_nbins_min_max, detailed_stats,
                                periodic_stat_period);

        // push back network and system
        network_apis.push_back(std::move(network_api));
        systems.push_back(system);
    }

    // Initiate ASTRA-sim simulation
    for (int i = 0; i < npus_count; i++) {
        systems[i]->workload->fire();
    }

    // run simulation
    while (!event_queue->finished()) {
        event_queue->proceed();
    }

    // delete systems
    for (int i = 0; i < npus_count; i++) {
        delete systems[i];
    }

    // terminate simulation
    return 0;
}
