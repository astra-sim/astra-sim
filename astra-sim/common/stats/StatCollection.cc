// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>

#include <astra-sim/common/stats/StatCollection.hh>
#include <astra-sim/common/stats/Statistics.hh>

namespace AstraSim {
StatCollection g_stat_collection;
std::chrono::time_point<std::chrono::high_resolution_clock> g_sim_start_time;
}  // namespace AstraSim

using namespace AstraSim;

void StatCollection::init(const std::string& filepath) {
    m_stat_filepath = filepath;
    g_sim_ticks.init(nullptr, "sim_ticks",
                     "Total number of ticks across all systems");
    g_sim_time.init(nullptr, "sim_time",
                    "Total simulation time (ms) across all systems");

    // Recording the start time
    g_sim_start_time = std::chrono::high_resolution_clock::now();
    m_sim_finished = false;
}

void StatCollection::regStat(SimObject* owner_p, Stat::Statistics* stat_p) {
    m_all_stats[owner_p].push_back(stat_p);
}

void StatCollection::dumpStats(bool sim_finished) {
    std::ofstream ofs(m_stat_filepath);
    if (!ofs.is_open()) {
        std::cerr << "Error in opening file: " << m_stat_filepath << std::endl;
        exit(-1);
    }

    // Recording the sim duration
    auto sim_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_ms =
        sim_end_time - g_sim_start_time;
    g_sim_time.sample(duration_ms.count());

    ofs << "# --- Begin of statistics ---\n";
    for (auto it = m_all_stats.begin(); it != m_all_stats.end(); ++it) {
        std::string owner_name;
        if (it->first == nullptr) {
            owner_name = "global";
        } else {
            owner_name = it->first->name();
        }
        for (auto stat_p : it->second) {
            if (stat_p->needDump()) {
                stat_p->dump(ofs, owner_name);
            }
        }
    }
    ofs << "# --- End of statistics ---\n";
    ofs.close();

    if (sim_finished) {
        assert(!m_sim_finished);
        m_sim_finished = true;
    }
}

bool StatCollection::isSimFinished() const {
    return m_sim_finished;
}
