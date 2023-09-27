/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <cxxopts/cxxopts.hpp>
#include <cstdlib>
#include <iostream>

namespace AstraSimAnalytical {

class CmdLineParser {
 public:
  explicit CmdLineParser(const char* argv0) noexcept;

  void parse(int argc, char* argv[]) noexcept;

  template <typename T>
  T get(const std::string& name) const noexcept {
    try {
      // try retrieving options
      return parsed[name].as<T>();
    } catch (const cxxopts::OptionException& e) {
      // error occured
      std::cerr << "[Error] (AstraSim/analytical/common) "
                << "Error parsing options: " << e.what() << std::endl;
      exit(-1);
    }
  }

 private:
  cxxopts::Options options;

  cxxopts::ParseResult parsed;

  void define_options() noexcept;
};

} // namespace AstraSimAnalytical
