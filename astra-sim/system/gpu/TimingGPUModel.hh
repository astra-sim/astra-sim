// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/gpu/GPUModel.hh>

namespace AstraSim {

class TimingGPUModel : public GPUModel {
  public:
    TimingGPUModel(GpuSys* sys,
                   Workload* wl,
                   int gpu_id,
                   enum Memory::MemoryMode memory_mode);
    ~TimingGPUModel();

    bool canLaunch(uint32_t node_id, uint32_t num_wgs) override;
    void launch(const Msccl::GpuParser& msccl_parser,
                Kernel& kernel,
                uint32_t wg_id) override;

    // Called by a workgroup to indicate it has reached a barrier
    void reachWorkgroupBarrier(TimingWorkgroup* wg_p,
                               uint32_t barrier_id,
                               uint32_t num_wgs_in_barrier);

    // Return pointer to TimingGPUModel given a GPU ID
    static TimingGPUModel* getGpuModelPtr(uint32_t gpu_id);

  private:
    const uint32_t m_num_cus;             // number of CUs
    const uint32_t m_num_threads_per_wf;  // number of threads per wavefront

    // List of compute units in this GPU
    std::vector<ComputeUnit*> m_cu_lst;

    // Collection of compute units we will schedule workgroups onto in the same
    // cycle
    std::vector<ComputeUnit*> m_sched_cu_lst;

    // ID of the next CU that a ready WG can be issued to
    uint32_t m_next_cu_id;

    // Return the next available CU's ID and update the next XCC. If none is
    // available, return the total number of CUs
    uint32_t getNextAvailCU();
};

}  // namespace AstraSim
