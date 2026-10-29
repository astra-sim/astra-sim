// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>
#include <cmath>

#include <astra-sim/common/stats/Statistics.hh>

#include <astra-sim/common/stats/StatCollection.hh>

namespace AstraSim {
Stat::SampledValue<Tick> g_sim_ticks;
Stat::SampledValue<double> g_sim_time;
}  // namespace AstraSim

using namespace AstraSim;

/**
 * Base statistics class
 */
void Stat::Statistics::init(SimObject* owner_p,
                            const std::string& _name,
                            const std::string& _desc) {
    name = _name;
    desc = _desc;
    g_stat_collection.regStat(owner_p, this);
}

bool Stat::Statistics::needDump() const {
    return true;
}

/**
 * Simple counter stats
 */
void Stat::Counter::init(SimObject* owner_p,
                         const std::string& name,
                         const std::string& desc,
                         uint64_t init_val,
                         bool dump_on_zero) {
    Stat::Statistics::init(owner_p, name, desc);
    m_val = init_val;
    m_dump_on_zero = dump_on_zero;
}

bool Stat::Counter::needDump() const {
    return m_dump_on_zero || (m_val > 0);
}

void Stat::Counter::dump(std::ofstream& os, const std::string& owner) const {
    os << std::setw(50) << std::left << owner + "." + name << std::setw(20)
       << std::right << m_val << " # " << desc << std::endl;
}

uint64_t Stat::Counter::getValue() const {
    return m_val;
}

Stat::Counter& Stat::Counter::operator++() {
    ++m_val;
    return *this;
}

Stat::Counter Stat::Counter::operator++(int) {
    auto temp = *this;
    ++m_val;
    return temp;
}

Stat::Counter& Stat::Counter::operator+=(const Counter& other) {
    m_val += other.m_val;
    return *this;
}

Stat::Counter& Stat::Counter::operator+=(uint64_t add_val) {
    m_val += add_val;
    return *this;
}

/**
 * 2D Counter Stats
 */
void Stat::TwoDCounter::init(SimObject* owner_p,
                             const std::string& name,
                             const std::string& desc) {

    Stat::Statistics::init(owner_p, name, desc);
}

void Stat::TwoDCounter::dump(std::ofstream& os,
                             const std::string& owner) const {
    for (auto outer_it = m_val_mat.begin(); outer_it != m_val_mat.end();
         ++outer_it) {
        auto i = outer_it->first;
        for (auto inner_it = m_val_mat.at(i).begin();
             inner_it != m_val_mat.at(i).end(); ++inner_it) {
            auto j = inner_it->first;
            auto val = inner_it->second;
            os << std::setw(50) << std::left
               << owner + "." + name + "." + std::to_string(i) + "." +
                      std::to_string(j)
               << std::setw(20) << std::right << val << " # " << desc
               << std::endl;
        }
    }
}

uint64_t Stat::TwoDCounter::getValue(int i, int j) const {
    return m_val_mat.at(i).at(j);
}

void Stat::TwoDCounter::incrementValue(int i, int j) {
    // create if needed
    m_val_mat[i][j]++;
}

void Stat::TwoDCounter::incrementValue(int i, int j, uint64_t val) {
    // create if needed
    m_val_mat[i][j] += val;
}

void Stat::TwoDCounter::setValue(int i, int j, uint64_t val) {
    // create if needed
    m_val_mat[i][j] = val;
}

/**
 * Histogram stats
 */

void Stat::Histogram::init(SimObject* owner_p,
                           const std::string& name,
                           const std::string& desc,
                           uint32_t num_bins,
                           double min_val,
                           double max_val) {
    Stat::Statistics::init(owner_p, name, desc);
    m_num_bins = num_bins;
    m_min_val = min_val;
    m_max_val = max_val;
    assert(m_num_bins > 0);
    assert(m_max_val > m_min_val);
    m_interval = (m_max_val - m_min_val) / m_num_bins;
    m_counts.resize(num_bins, 0);
}

void Stat::Histogram::dump(std::ofstream& os, const std::string& owner) const {
    for (auto i = 0; i < m_counts.size(); ++i) {
        os << std::setw(50) << std::left
           << owner + "." + name + ":" + std::to_string(i) << std::setw(20)
           << std::right << m_counts[i] << " # " << desc;
        // print range
        if (i == 0) {
            os << " (-inf, " << std::fixed << std::setprecision(2)
               << (m_min_val + (i + 1) * m_interval) << ")";
        } else if (i == m_counts.size() - 1) {
            os << " (" << std::fixed << std::setprecision(2)
               << (m_min_val + i * m_interval) << ", "
               << "+inf)";
        } else {
            os << " (" << std::fixed << std::setprecision(2)
               << (m_min_val + i * m_interval) << ", " << std::fixed
               << std::setprecision(2) << (m_min_val + (i + 1) * m_interval)
               << ")";
        }
        os << std::endl;
    }
}

void Stat::Histogram::sample(double value) {
    if (value < m_min_val) {
        m_counts[0]++;
    } else if (value > m_max_val) {
        m_counts[m_num_bins - 1]++;
    } else {
        uint32_t bin = std::floor((value - m_min_val) / m_interval);
        m_counts[bin]++;
    }
}
