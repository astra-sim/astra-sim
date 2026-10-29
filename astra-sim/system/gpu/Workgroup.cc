// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>

#include <astra-sim/common/logging/EventTracer.hh>
#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/system/gpu/ComputeUnit.hh>
#include <astra-sim/system/gpu/FunctionalGPUModel.hh>
#include <astra-sim/system/gpu/TimingGPUModel.hh>
#include <astra-sim/system/gpu/Workgroup.hh>

using namespace AstraSim;

Workgroup::Workgroup(uint32_t gpu_id,
                     uint32_t kernel_id,
                     uint32_t wg_id,
                     GpuOpPtr first_wg_op_p)
    : m_gpu_id(gpu_id),
      m_kernel_id(kernel_id),
      m_wg_id(wg_id),
      m_gpu_op(first_wg_op_p) {}

uint32_t Workgroup::getGpuId() const {
    return m_gpu_id;
}

uint32_t Workgroup::getKernelId() const {
    return m_kernel_id;
}

uint32_t Workgroup::getWorkgroupId() const {
    return m_wg_id;
}

void Workgroup::execute() {
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, Workgroup* wg) {
            os << "Calling Workgroup::execute() on " << wg->to_string();
        },
        this);

    // Functional execution, if this method is not overridden.
    bool begin_completed = (m_gpu_op == nullptr);
    FunctionalGPUModel* gpu = FunctionalGPUModel::getGpuModelPtr(m_gpu_id);
    while (m_gpu_op != nullptr && m_gpu_op->executeFunctional(gpu, this)) {
        // Execution succeeded, move on to next op.
        m_gpu_op = m_gpu_op->getNextOpPtr();
    }

    // At this point, we either completed all GpuOp's, or we encountered some
    // kind of blocker preventing us from proceeding further (e.g., a semaphore
    // acquire which didn't succeed). This "execute()" function may invoke later
    // on to pick up where we left off.
    if (!begin_completed && m_gpu_op == nullptr) {
        // Became complete during this invocation.
        gpu->completeWorkgroup(this);
    }
}

bool Workgroup::isCompleted() const {
    // This should be nullptr when there is nothing left to execute.
    return m_gpu_op == nullptr;
}

std::string Workgroup::to_string() const {
    std::stringstream ss;
    ss << "[Workgroup] kernel=" << m_kernel_id;
    ss << ",wg=" << m_wg_id;
    ss << ",gpu=" << m_gpu_id;
    ss << ",op=" << (m_gpu_op == nullptr ? "nullptr" : m_gpu_op->to_string());
    return ss.str();
}

TimingWorkgroup::TimingWorkgroup(uint32_t gpu_id,
                                 uint32_t kernel_id,
                                 uint32_t wg_id,
                                 uint32_t num_wfs,
                                 GpuOpPtr first_wg_op_p,
                                 ComputeUnit* cu_p)
    : Workgroup(gpu_id, kernel_id, wg_id, first_wg_op_p),
      m_is_executed(false),
      m_num_wfs(num_wfs),
      m_cu_p(cu_p) {
    assert(m_num_wfs > 0);
    assert(m_cu_p);

    // Construct wavefronts - make sure to reserve space to avoid reallocation
    m_wf_lst.reserve(m_num_wfs);
    for (uint32_t wf_idx = 0; wf_idx < m_num_wfs; ++wf_idx) {
        m_wf_lst.emplace_back(this, wf_idx, m_num_wfs, first_wg_op_p);
    }
}

void TimingWorkgroup::execute() {
    // Dispatch the first set of ready wfreqs for all wavefronts.
    for (auto& wf : m_wf_lst) {
        wf.dispatchWFReqs();
    }

    // All WFs are active now
    m_num_outstanding_wfs = m_num_wfs;
    m_is_executed = true;
}

bool TimingWorkgroup::isCompleted() const {
    return m_is_executed && m_num_outstanding_wfs == 0;
}

void TimingWorkgroup::execNopOp(Wavefront* wf_p) {
    assert(wf_p);
    auto wf_id = wf_p->getWfId();
    assert(wf_id < m_wf_lst.size());
    assert(m_stalled_wf_ids.count(wf_id) == 0);

    // Insert this wavefront ID into the stalled set
    m_stalled_wf_ids.insert(wf_id);
    wf_p->stall();
    DPRINTFNR("Stalled WF at Nop: gpu=%d, kernel=%d, wg=%d, wf=%d\n", m_gpu_id,
              m_kernel_id, m_wg_id, wf_id);

    // If all wavefronts are stalled, we can unstall them now
    if (m_stalled_wf_ids.size() == m_wf_lst.size()) {
        // Call CU to schedule an event to unstall all WFs of this WG
        m_cu_p->schedWorkgroupUnstallEvent(this);
    }
}

void TimingWorkgroup::execBarrierOp(Wavefront* wf_p,
                                    uint32_t barrier_id,
                                    uint32_t num_wgs_in_barrier) {
    assert(wf_p);
    auto wf_id = wf_p->getWfId();
    assert(wf_id < m_wf_lst.size());
    assert(m_stalled_wf_ids.count(wf_id) == 0);

    // Insert this wavefront ID into the stalled set
    m_stalled_wf_ids.insert(wf_id);
    wf_p->stall();
    DPRINTFNR("Stalled WF at Barrier (%d, %d): gpu=%d, kernel=%d, wg=%d, "
              "wf=%d\n",
              barrier_id, num_wgs_in_barrier, m_gpu_id, m_kernel_id, m_wg_id,
              wf_id);

    // If all wavefronts are stalled, we can notify the GPUModel that this WG
    // has reached the barrier
    if (m_stalled_wf_ids.size() == m_wf_lst.size()) {
        m_cu_p->getGpuModelPtr()->reachWorkgroupBarrier(this, barrier_id,
                                                        num_wgs_in_barrier);
    }
}

void TimingWorkgroup::completeWavefront(uint32_t wf_id) {
    assert(wf_id < m_wf_lst.size());
    assert(m_wf_lst[wf_id].isCompleted());
    assert(m_num_outstanding_wfs > 0);
    m_num_outstanding_wfs--;
}

void TimingWorkgroup::unstall() {
    assert(m_stalled_wf_ids.size() == m_wf_lst.size());
    for (auto& wf : m_wf_lst) {
        wf.unstall();
    }
    m_stalled_wf_ids.clear();
}

uint32_t TimingWorkgroup::getNumWavefronts() const {
    return m_num_wfs;
}

uint32_t TimingWorkgroup::getCuId() const {
    return m_cu_p->getId();
}

uint32_t TimingWorkgroup::getNumOutstandingWavefronts() const {
    return m_num_outstanding_wfs;
}

Wavefront* TimingWorkgroup::getWavefrontPtr(uint32_t wf_id) {
    assert(wf_id < m_wf_lst.size());
    return &(m_wf_lst[wf_id]);
}

ComputeUnit* TimingWorkgroup::getComputeUnitPtr() const {
    return m_cu_p;
}

std::string TimingWorkgroup::to_string() const {
    std::stringstream ss;
    ss << "[TimingWorkgroup] kernel=" << m_kernel_id;
    ss << ",wg=" << m_wg_id;
    ss << ",num_outstanding_wfs=" << m_num_outstanding_wfs;
    return ss.str();
}
