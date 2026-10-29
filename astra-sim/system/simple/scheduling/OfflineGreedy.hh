// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <vector>

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/simple/SimpleSys.hh>

namespace AstraSim {

class DimElapsedTime {
  public:
    int dim_num;
    double elapsed_time;
    DimElapsedTime(int dim_num);
    bool operator<(const DimElapsedTime& dimElapsedTime) const {
        return (elapsed_time < dimElapsedTime.elapsed_time);
    }
};
class OfflineGreedy {
  public:
    SimpleSys* sys;
    std::vector<DimElapsedTime> dim_elapsed_time;
    std::vector<double> dim_BW;
    std::vector<int> dim_size;
    OfflineGreedy(SimpleSys* sys);
    void reset_loads();
    std::vector<int> get_chunk_scheduling(
        long long chunk_id,
        uint64_t& remaining_data_size,
        uint64_t recommended_chunk_size,
        std::vector<bool>& dimensions_involved,
        InterDimensionScheduling inter_dim_scheduling,
        ComType comm_type);
    uint64_t get_chunk_size_from_elapsed_time(double elapsed_time,
                                              DimElapsedTime dim,
                                              ComType comm_type);
    static std::map<long long, std::vector<int>> chunk_schedule;
    static std::map<long long, int> schedule_consumer;
    static std::map<long long, uint64_t> global_chunk_size;

    // The OfflineGreedy belonging to rank 0, which computes the chunk
    // schedule that every other rank's get_chunk_scheduling() delegates to.
    static OfflineGreedy* s_rank0;
};

}  // namespace AstraSim
