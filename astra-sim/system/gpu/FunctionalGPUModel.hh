// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/gpu/GPUModel.hh>

namespace AstraSim {

class FunctionalGPUModel : public GPUModel {
  public:
    FunctionalGPUModel(GpuSys* sys,
                       Workload* wl,
                       int gpu_id,
                       enum Memory::MemoryMode memory_mode);

    bool canLaunch(uint32_t node_id, uint32_t num_wgs) override;
    void launch(const Msccl::GpuParser& msccl_parser,
                Kernel& kernel,
                uint32_t wg_id) override;

    // Return pointer to FunctionalGPUModel given a GPU ID
    static FunctionalGPUModel* getGpuModelPtr(uint32_t gpu_id);
};

}  // namespace AstraSim
