// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <common/CmdLineParser.hh>

using namespace AstraSimAnalytical;

CmdLineParser::CmdLineParser(const char* const argv0) noexcept
    : options(argv0, "ASTRA-sim") {
    parsed = {};

    // define options
    define_options();
}

void CmdLineParser::define_options() noexcept {
    options.set_width(70).allow_unrecognised_options().add_options()(
        "workload-configuration", "Workload configuration file",
        cxxopts::value<std::string>())(
        "comm-group-configuration", "Communicator group configuration fi`le",
        cxxopts::value<std::string>()->default_value("empty"))(
        "system-configuration", "System configuration file",
        cxxopts::value<std::string>())("remote-memory-configuration",
                                       "Remote memory configuration file",
                                       cxxopts::value<std::string>())(
        "network-configuration", "Network configuration file",
        cxxopts::value<std::string>())(
        "num-queues-per-dim", "Number of queues per each dimension",
        cxxopts::value<int>()->default_value("1"))(
        "compute-scale", "Compute scale",
        cxxopts::value<double>()->default_value("1"))(
        "comm-scale", "Communication scale",
        cxxopts::value<double>()->default_value("1"))(
        "injection-scale", "Injection scale",
        cxxopts::value<double>()->default_value("1"))(
        "rendezvous-protocol", "Whether to enable rendezvous protocol",
        cxxopts::value<bool>()->default_value("false"))(
        "comm-size",
        "Per-GPU input buffer size in bytes for a single custom collective "
        "(MSCCL++ plan) workload",
        cxxopts::value<uint64_t>()->default_value("0"))(
        "custom-comm-plan-map",
        "YAML file mapping Chakra node IDs to custom collective (MSCCL++) "
        "plan files",
        cxxopts::value<std::string>()->default_value(""))(
        "stat-file", "Simulation statistics file",
        cxxopts::value<std::string>())(
        "detailed-network-stats",
        "Simulation statistics on link usage and buffer occupancy",
        cxxopts::value<bool>()->default_value("false"))(
        "periodic-stats-period",
        "If using detailed stats the period for periodic stats",
        cxxopts::value<int>()->default_value("10000"));
}

void CmdLineParser::parse(int argc, char* argv[]) noexcept {
    try {
        // try parsing command line options
        parsed = options.parse(argc, argv);
    } catch (const cxxopts::OptionException& e) {
        // error occurred
        std::cerr << "[Error] (AstraSim/analytical/common) "
                  << "Error parsing options: " << e.what() << std::endl;
        exit(-1);
    }
}

cxxopts::Options& CmdLineParser::get_options() {
    return options;
}
