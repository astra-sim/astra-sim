/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __USAGE_TRACKER_HH__
#define __USAGE_TRACKER_HH__

#include <cstdint>

#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/Usage.hh"
#include "astra-sim/system/CSVWriter.hh"

namespace AstraSim {

class UsageTracker {
 public:
  UsageTracker(int levels);
  void increase_usage();
  void decrease_usage();
  void set_usage(int level);
  void report(CSVWriter* writer, int offset);
  std::list<std::pair<uint64_t, double> > report_percentage(uint64_t cycles);

  int levels;
  int current_level;
  Tick last_tick;
  std::list<Usage> usage;
};

} // namespace AstraSim

#endif /* __USAGE_TRACKER_HH__ */
