// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/gpu/FunctionalGPUModel.hh>
#include <astra-sim/system/gpu/GpuSys.hh>
#include <astra-sim/system/gpu/msccl/Parser.hh>

using namespace AstraSim;

FunctionalGPUModel::FunctionalGPUModel(GpuSys* sys,
                                       Workload* wl,
                                       int gpu_id,
                                       enum Memory::MemoryMode memory_mode)
    : GPUModel(sys, wl, gpu_id, memory_mode, timespec_t(time_type_e::PS, 0)) {
    // Track this GPU model in the static list
    if (gpu_id != s_gpu_model_lst.size()) {
        THROW_ERR("GPU IDs must be assigned sequentially starting from 0. "
                  "gpu_id=%d, current_size=%d\n",
                  gpu_id, s_gpu_model_lst.size());
    }
    s_gpu_model_lst.push_back(this);
}

FunctionalGPUModel* FunctionalGPUModel::getGpuModelPtr(uint32_t gpu_id) {
    assert(gpu_id < s_gpu_model_lst.size());
    FunctionalGPUModel* model_p =
        dynamic_cast<FunctionalGPUModel*>(s_gpu_model_lst[gpu_id]);
    assert(model_p != nullptr);
    return model_p;
}

bool FunctionalGPUModel::canLaunch(uint32_t node_id, uint32_t num_wgs) {
    // We have no notion of hardware restrictions, so there is nothing blocking
    // us from launching a workgroup. A semaphore may be restricting our ability
    // to run, but that semaphore logic is handled in a different code path, so
    // in *this* function we should always indicate we're able to launch.
    return true;
}

void FunctionalGPUModel::launch(const Msccl::GpuParser& msccl_parser,
                                Kernel& kernel,
                                uint32_t wg_id) {
    // Construct WG and execute it functionally.
    GpuOpPtr first_op_p = msccl_parser.getFirstGpuOp(wg_id);
    assert(kernel.wg_lst.count(wg_id) == 0);
    kernel.wg_lst[wg_id] =
        new Workgroup(m_id,            // GPU ID
                      kernel.node_id,  // Kernel ID (using node ID here)
                      wg_id,           // WG ID
                      first_op_p);     // first WG op

    // Execute this workgroup one cycle in the future. We don't want to begin
    // any execution if its possible the other GPUs we interact with aren't
    // initialized yet, so give them the first cycle to set up.
    m_sys_p->comm_NI->sim_schedule(
        timespec_t{time_type_e::PS, 1}, &Sys::handleEvent,
        new WorkgroupExecHandlerData(m_sys_p->id, kernel.wg_lst[wg_id]));
}
