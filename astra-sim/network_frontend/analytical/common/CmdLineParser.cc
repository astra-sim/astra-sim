/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/CmdLineParser.hh"

using namespace AstraSimAnalytical;

CmdLineParser::CmdLineParser(const char* const argv0) noexcept
    : options(argv0, "ASTRA-sim") {
  parsed = {};

  // define options
  define_options();
}

void CmdLineParser::define_options() noexcept {
  options.set_width(70).allow_unrecognised_options().add_options()(
      "workload-configuration",
      "Workload configuration file",
      cxxopts::value<std::string>())(
      "comm-group-configuration",
      "Communicator group configuration fi`le",
      cxxopts::value<std::string>()->default_value("empty"))(
      "system-configuration",
      "System configuration file",
      cxxopts::value<std::string>())(
      "remote-memory-configuration",
      "Remote memory configuration file",
      cxxopts::value<std::string>())(
      "network-configuration",
      "Network configuration file",
      cxxopts::value<std::string>())(
      "logging-configuration",
      "Logging configuration file",
      cxxopts::value<std::string>()->default_value("empty"))(
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
