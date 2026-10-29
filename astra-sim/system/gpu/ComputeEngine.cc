// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/gpu/ComputeEngine.hh>

#include <astra-sim/system/gpu/ComputeUnit.hh>
#include <astra-sim/system/gpu/GPUModel.hh>
#include <astra-sim/system/gpu/GpuOp.hh>
#include <astra-sim/system/gpu/GpuSys.hh>

using namespace AstraSim;

ComputeEngine::ComputeEngine(ComputeUnit* cu_p,
                             uint32_t simd_latency,
                             uint32_t simd_bandwidth)
    : m_name(cu_p->name() + ".simd"),
      m_cu_p(cu_p),
      m_simd_latency(simd_latency),
      m_simd_bandwidth(simd_bandwidth),
      m_next_avail_tick(0) {
    m_stat_blocked_ticks.init(
        this, "blocked_ticks",
        "Number of ticks for which reqs are blocked due to limit BW");
}

void ComputeEngine::call(EventType type, CallData* data) {
    auto call_data_p = dynamic_cast<CE_Event_CallData*>(data);
    assert(call_data_p);

    if (type == EventType::CE_ExecWFReq) {
        auto wfreq_p = call_data_p->wfreq_p;
        assert(wfreq_p != nullptr);
        m_cu_p->scheduleEventInCycles(
            EventType::CU_WakeupWFReq,
            new ComputeUnit::CU_Event_CallData(wfreq_p), 1);
        DPRINTF("Completed %s\n", wfreq_p->to_string());
    } else {
        THROW_ERR("Invalid ComputeEngine event type: %d\n",
                  static_cast<int>(type));
    }

    // clean up
    if (data != nullptr) {
        delete data;
    }
}

const std::string& ComputeEngine::name() const {
    return m_name;
}

Tick ComputeEngine::curTick() const {
    return m_cu_p->curTick();
}

bool ComputeEngine::send(WFReqPtr wfreq_p) {
    assert(wfreq_p->inst == GpuInst::Reduce);

    // Clock period in ticks
    Tick clock_period_ticks = m_cu_p->getSysPtr()->getClockPeriodInTicks();

    // Compute issue latency and overall execution latency
    // Here we assume each wfreq has a cache-line-size worth of data for
    // computation
    uint32_t issue_cycles = GPUModel::CLINE_NBYTES / m_simd_bandwidth;
    uint32_t exec_cycles = issue_cycles + m_simd_latency;

    // Tick when this request can be issued
    Tick issue_tick =
        (curTick() < m_next_avail_tick) ? m_next_avail_tick : curTick();
    Tick complete_tick = issue_tick + exec_cycles * clock_period_ticks;

    // Schedule an event to complete this req after exec_cycles from now
    m_cu_p->getSysPtr()->register_event(this, EventType::CE_ExecWFReq,
                                        new CE_Event_CallData(wfreq_p),
                                        complete_tick - curTick());

    // Update the next available tick when the next request can be issued
    m_next_avail_tick = issue_tick + issue_cycles * clock_period_ticks;

    DPRINTF("Processed %s: issue_tick=%d, complete_tick=%d\n",
            wfreq_p->to_string(), issue_tick, complete_tick);

    // Update stat
    m_stat_blocked_ticks.sample(issue_tick - curTick());

    // Always take in requests, so no backpressure
    return true;
}

void ComputeEngine::scheduleEventInCycles(EventType event_type,
                                          CallData* call_data_p,
                                          uint64_t cycles) {
    m_cu_p->getSysPtr()->registerEventInCycles(this, event_type, call_data_p,
                                               cycles);
}
