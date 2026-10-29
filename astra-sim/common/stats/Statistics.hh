// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <vector>

#include <astra-sim/common/SimObject.hh>

namespace AstraSim {
namespace Stat {

/**
 * Base statistics class
 */
class Statistics {
  public:
    virtual ~Statistics() = default;

    // Init and register this stats to the stat collection
    virtual void init(SimObject* owner_p,
                      const std::string& name,
                      const std::string& desc);
    // Return true if this stats need to be dumped
    virtual bool needDump() const;

    // Dump out this statistics into a given output file stream. This is
    // typically call at the end of a simulation to dump out all stats
    virtual void dump(std::ofstream& os, const std::string& owner) const = 0;

  protected:
    // Name of the stat
    std::string name;
    // Description of the stat
    std::string desc;
};

/**
 * SampledValue
 */
template <typename T> class SampledValue : public Statistics {
  public:
    SampledValue() = default;
    ~SampledValue() = default;

    void init(SimObject* owner_p,
              const std::string& name,
              const std::string& desc,
              bool dump_on_zero = false);

    bool needDump() const override;

    void dump(std::ofstream& os, const std::string& owner) const override;

    // Update the current value to a new value
    void sample(const T& val);

  private:
    T m_max = 0;
    T m_min = 0;
    T m_sum = 0;
    uint64_t m_count = 0;
    bool m_dump_on_zero = false;
};

template <typename T>
void SampledValue<T>::init(SimObject* owner_p,
                           const std::string& name,
                           const std::string& desc,
                           bool dump_on_zero) {
    Statistics::init(owner_p, name, desc);
    m_dump_on_zero = dump_on_zero;
}

template <typename T> bool SampledValue<T>::needDump() const {
    return m_dump_on_zero || (m_count > 0);
}

template <typename T>
void SampledValue<T>::dump(std::ofstream& os, const std::string& owner) const {
    os << std::setw(50) << std::left << owner + "." + name + ".avg";
    if (m_count > 0) {
        os << std::setw(20) << std::right << (m_sum / (double)m_count);
    } else {
        os << std::setw(20) << std::right << "nan";
    }
    os << " # " << desc << " (average)" << std::endl;
    os << std::setw(50) << std::left << owner + "." + name + ".cnt"
       << std::setw(20) << std::right << m_count << " # " << desc << " (count)"
       << std::endl;
    os << std::setw(50) << std::left << owner + "." + name + ".max"
       << std::setw(20) << std::right << m_max << " # " << desc << " (max)"
       << std::endl;
    os << std::setw(50) << std::left << owner + "." + name + ".min"
       << std::setw(20) << std::right << m_min << " # " << desc << " (min)"
       << std::endl;
}

template <typename T> void SampledValue<T>::sample(const T& val) {
    m_sum += val;
    if (m_count == 0) {
        m_max = val;
        m_min = val;
    } else if (val > m_max) {
        m_max = val;
    } else if (val < m_min) {
        m_min = val;
    }
    m_count++;
}

/**
 * Counter stats
 */
class Counter : public Statistics {
  public:
    Counter() = default;
    ~Counter() = default;

    void init(SimObject* owner_p,
              const std::string& name,
              const std::string& desc,
              uint64_t init_val = 0,
              bool dump_on_zero = false);
    bool needDump() const override;
    void dump(std::ofstream& os, const std::string& owner) const override;

    // Return current value
    uint64_t getValue() const;
    // Prefix increment operator
    Counter& operator++();
    // Postfix increment operator
    Counter operator++(int);
    // += Counter
    Counter& operator+=(const Counter& other);
    // += uint64_t
    Counter& operator+=(uint64_t add_val);

  private:
    // Counter value
    uint64_t m_val;
    // True if we want to dump when m_val is zero
    bool m_dump_on_zero = false;
};

/**
 * 2D sparse matrix stats
 */
class TwoDCounter : public Statistics {
  public:
    TwoDCounter() = default;
    ~TwoDCounter() = default;

    void init(SimObject* owner_p,
              const std::string& name,
              const std::string& desc) override;

    void dump(std::ofstream& os, const std::string& owner) const override;

    // Return value @ (i,j)
    uint64_t getValue(int i, int j) const;

    // Increment (by 1) value @ (i,j)
    void incrementValue(int i, int j);

    // Increment (by val) value @ (i,j)
    void incrementValue(int i, int j, uint64_t val);

    // Set (to val) value @ (i,j)
    void setValue(int i, int j, uint64_t val);

  private:
    // Counter value
    // map to let it be sparse
    std::unordered_map<int, std::unordered_map<int, uint64_t>> m_val_mat;
};

/**
 * Histogram stats
 */
class Histogram : public Statistics {
  public:
    Histogram() = default;
    ~Histogram() = default;

    void init(SimObject* owner_p,
              const std::string& name,
              const std::string& desc,
              uint32_t num_bins,
              double min_val,
              double max_val);
    void dump(std::ofstream& os, const std::string& owner) const override;
    void sample(double val);

  private:
    uint32_t m_num_bins;
    double m_min_val;
    double m_max_val;
    double m_interval;
    std::vector<uint64_t> m_counts;
};

}  // end of namespace Stat

extern Stat::SampledValue<Tick> g_sim_ticks;
extern Stat::SampledValue<double> g_sim_time;

}  // end of namespace AstraSim
