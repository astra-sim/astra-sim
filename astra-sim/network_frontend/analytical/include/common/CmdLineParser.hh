/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <cstdlib>
#include <iostream>
#include "astra-sim/common/Logging.hh"
#include "cxxopts/cxxopts.hpp"

namespace AstraSimAnalytical {

/**
 * CmdLineParser parses command line arguments using cxxopts
 * for ASTRA-sim input parameters.
 */
class CmdLineParser {
 public:
  /**
   * Constructor.
   *
   * @param argv0 argv[0] from main function
   */
  explicit CmdLineParser(const char* argv0) noexcept;

  /**
   * Parse the command line arguments.
   *
   * @param argc argc from main function
   * @param argv argv from main function
   */
  void parse(int argc, char* argv[]) noexcept;

  /**
   * Get the value of the option in type T with the given name.
   *
   * @tparam T type of the argument value
   * @param name name of the argument
   * @return read argument value in type T
   */
  template <typename T>
  T get(const std::string& name) const noexcept {
    try {
      // try retrieving options
      return parsed[name].as<T>();
    } catch (const cxxopts::OptionException& e) {
      // error occured
      auto logger = AstraSim::Logger::getLogger(
          "network_frontend::analytical::CmdLineParser");
      logger->critical("Error parsing options: {}", e.what());
      exit(-1);
    }
  }

 private:
  /// cxxopts options
  cxxopts::Options options;

  /// cxxopts parsed results
  cxxopts::ParseResult parsed;

  /**
   * Define the options for cxxopts.
   */
  void define_options() noexcept;
};

} // namespace AstraSimAnalytical
