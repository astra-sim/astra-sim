// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/common/WorkloadLayerHandlerData.hh>
#include <astra-sim/system/gpu/ComputeUnit.hh>
#include <astra-sim/system/gpu/GpuSys.hh>
#include <astra-sim/system/gpu/TimingGPUModel.hh>
#include <astra-sim/system/gpu/msccl/Parser.hh>

using namespace AstraSim;

TimingGPUModel::TimingGPUModel(GpuSys* sys,
                               Workload* wl,
                               int gpu_id,
                               enum Memory::MemoryMode memory_mode)
    : GPUModel(
          sys, wl, gpu_id, memory_mode, timespec_t(time_type_e::PS, 10000)),
      m_num_cus(sys->num_cus),
      m_num_threads_per_wf(sys->num_threads_per_wf),
      m_next_cu_id(0) {
    assert(m_num_threads_per_wf > 0);

    // Track this GPU model in the static list
    if (gpu_id != s_gpu_model_lst.size()) {
        THROW_ERR("GPU IDs must be assigned sequentially starting from 0. "
                  "gpu_id=%d, current_size=%d\n",
                  gpu_id, s_gpu_model_lst.size());
    }
    s_gpu_model_lst.push_back(this);

    // Construct all compute units
    for (uint32_t i = 0; i < m_num_cus; ++i) {
        auto cu_p = new ComputeUnit(this, sys, i);
        m_cu_lst.push_back(cu_p);
    }

    // Init stats
    m_stat_num_nodes.init(this, "total_num_nodes",
                          "Total number of nodes received by this GPU");
    m_stat_num_wgs.init(this, "num_workgroups", "Total number of workgroups");
}

TimingGPUModel::~TimingGPUModel() {
    for (auto p : m_cu_lst) {
        delete p;
    }
}

bool TimingGPUModel::canLaunch(uint32_t node_id, uint32_t num_wgs) {
    // Schedule WGs to available CUs
    // TODO: Currently, for simplicity, we assume all WGs of a kernel node need
    // to be scheduled to CUs at once. If there is no enough available CUs for
    // _ALL_ WGs, we will need to do the scheduling again later when more CUs
    // are free.
    m_sched_cu_lst = std::vector<ComputeUnit*>(num_wgs, nullptr);
    for (auto wg_id = 0; wg_id < num_wgs; ++wg_id) {
        auto cu_id = getNextAvailCU();
        if (cu_id < m_num_cus) {
            m_sched_cu_lst[wg_id] = m_cu_lst[cu_id];
            // Update the next CU ID
            m_next_cu_id = (cu_id + 1) % m_num_cus;
        } else {
            DPRINTF("Not enough available CUs to schedule all WGs of "
                    "node_id=%d. WG_id=%d cannot be scheduled now.\n",
                    node_id, wg_id);
            return false;
        }
    }
    return true;
}

void TimingGPUModel::launch(const Msccl::GpuParser& msccl_parser,
                            Kernel& kernel,
                            uint32_t wg_id) {
    auto num_threads_per_wg = msccl_parser.num_threads_per_tb;
    assert(num_threads_per_wg > 0);
    if (num_threads_per_wg % m_num_threads_per_wf != 0) {
        THROW_ERR("Number of threads per workgroup (%d) is not a multiple of "
                  "number of threads per wavefront (%d)\n",
                  num_threads_per_wg, m_num_threads_per_wf);
    }
    uint32_t num_wfs_per_wg = num_threads_per_wg / m_num_threads_per_wf;

    auto cu_p = m_sched_cu_lst[wg_id];
    assert(cu_p);
    // Construct WG
    GpuOpPtr first_op_p = msccl_parser.getFirstGpuOp(wg_id);
    assert(kernel.wg_lst.count(wg_id) == 0);
    kernel.wg_lst[wg_id] =
        new TimingWorkgroup(m_id,            // GPU ID
                            kernel.node_id,  // Kernel ID (using node ID here)
                            wg_id,           // WG ID
                            num_wfs_per_wg,  // number of WFs
                            first_op_p,      // first WG op
                            cu_p);           // pointer to the CU
    DPRINTF("Assigning wg_id=%d to CU %d\n", wg_id, cu_p->getId());

    // Dispatch it to the assigned CU
    cu_p->dispatchWorkgroup(
        dynamic_cast<TimingWorkgroup*>(kernel.wg_lst.at(wg_id)));
}

uint32_t TimingGPUModel::getNextAvailCU() {
    uint32_t i = 0;
    uint32_t cu_id = m_next_cu_id;
    while (i < m_num_cus && !m_cu_lst[cu_id]->isAvailForNewWG()) {
        cu_id = (cu_id + 1) % m_num_cus;
        i++;
    }

    if (i == m_num_cus) {
        return m_num_cus;  // no available CU
    } else {
        return cu_id;
    }
}

void TimingGPUModel::reachWorkgroupBarrier(TimingWorkgroup* wg_p,
                                           uint32_t barrier_id,
                                           uint32_t num_wgs_in_barrier) {
    assert(wg_p);
    assert(num_wgs_in_barrier > 0);

    if (m_outstanding_wg_barriers.count(barrier_id) == 0) {
        // Create a new barrier entry
        m_outstanding_wg_barriers.emplace(
            std::piecewise_construct, std::forward_as_tuple(barrier_id),
            std::forward_as_tuple(num_wgs_in_barrier));
    }

    auto& wg_barrier = m_outstanding_wg_barriers.at(barrier_id);
    assert(wg_barrier.waiting_wg_set.size() < wg_barrier.num_wgs);
    assert(wg_barrier.waiting_wg_set.count(wg_p) == 0);
    wg_barrier.waiting_wg_set.insert(wg_p);

    if (wg_barrier.waiting_wg_set.size() == wg_barrier.num_wgs) {
        // All WGs have reached this barrier, so we can unstall them now
        DPRINTF("All %d workgroups have reached barrier_id=%d. "
                "Unstalling them now.\n",
                wg_barrier.num_wgs, barrier_id);
        for (auto w : wg_barrier.waiting_wg_set) {
            TimingWorkgroup* timing_wg = dynamic_cast<TimingWorkgroup*>(w);
            assert(timing_wg != nullptr);
            auto cu_p = timing_wg->getComputeUnitPtr();
            cu_p->schedWorkgroupUnstallEvent(timing_wg);
        }
        // Remove this barrier entry
        m_outstanding_wg_barriers.erase(barrier_id);
    }
}

TimingGPUModel* TimingGPUModel::getGpuModelPtr(uint32_t gpu_id) {
    assert(gpu_id < s_gpu_model_lst.size());
    TimingGPUModel* model_p =
        dynamic_cast<TimingGPUModel*>(s_gpu_model_lst[gpu_id]);
    assert(model_p != nullptr);
    return model_p;
}
