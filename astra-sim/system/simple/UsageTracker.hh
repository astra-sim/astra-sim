// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cstdint>

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/simple/CSVWriter.hh>
#include <astra-sim/system/simple/Usage.hh>

namespace AstraSim {

class UsageTracker {
  public:
    UsageTracker(int levels);
    void increase_usage();
    void decrease_usage();
    void set_usage(int level);
    void report(CSVWriter* writer, int offset);
    std::list<std::pair<uint64_t, double>> report_percentage(uint64_t cycles);

    int levels;
    int current_level;
    Tick last_tick;
    std::list<Usage> usage;
};

}  // namespace AstraSim
