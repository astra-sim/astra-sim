// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <astra-sim/common/SimObject.hh>

namespace AstraSim {

namespace Stat {
class Statistics;
}

struct SimObjectPtrCompare {
    bool operator()(SimObject* a, SimObject* b) const {
        if (a == nullptr && b == nullptr) {
            return false;
        } else if (a == nullptr) {
            return false;
        } else if (b == nullptr) {
            return true;
        } else {
            return a->name() < b->name();
        }
    }
};

class StatCollection {
  public:
    StatCollection() = default;
    ~StatCollection() = default;

    // Init
    void init(const std::string& filepath);

    // Register a statistics in the collection
    void regStat(SimObject* owner_p, Stat::Statistics* stat_p);

    // Dump all stats to a file
    void dumpStats(bool sim_finished);

    // Return true if simulation has finished
    bool isSimFinished() const;

  private:
    // Output file stream for stats
    std::string m_stat_filepath;

    // Tracking all stats per simobject. All objects are sorted by their names
    std::map<SimObject*, std::vector<Stat::Statistics*>, SimObjectPtrCompare>
        m_all_stats;

    // True if simulation is finished
    bool m_sim_finished = false;
};

extern StatCollection g_stat_collection;
extern std::chrono::time_point<std::chrono::high_resolution_clock>
    g_sim_start_time;

}  // end of namespace AstraSim
