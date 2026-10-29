// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/SimObject.hh>
#include <astra-sim/common/stats/Statistics.hh>
#include <astra-sim/system/common/CallData.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/gpu/Wavefront.hh>

namespace AstraSim {

class ComputeUnit;

class ComputeEngine : public Callable, SimObject {
  public:
    /** CallData for ComputeEngine event */
    class CE_Event_CallData : public CallData {
      public:
        CE_Event_CallData(WFReqPtr p) : CallData(), wfreq_p(p) {}
        ~CE_Event_CallData() override = default;
        WFReqPtr wfreq_p;
    };
    ComputeEngine(ComputeUnit* cu_p, uint32_t simd_latency, uint32_t simd_bw);
    ~ComputeEngine() = default;

    // Call() for Callable
    void call(EventType type, CallData* data) override;

    // name() for SimObject
    const std::string& name() const override;

    // curTick() for SimObject
    Tick curTick() const override;

    // Schedule a simulatioin event
    void scheduleEventInCycles(EventType event_type,
                               CallData* call_data_p,
                               uint64_t cycles);

    // Send a compute request to this engine. Return true if the request can be
    // taken. Otherwise, return false, and the engine will do a callback.
    bool send(WFReqPtr wfreq_p);

  private:
    // Name for logging
    const std::string m_name;

    // Pointer to the compute unit owning this engine
    ComputeUnit* m_cu_p;

    // SIMD latency (in cycles)
    const uint32_t m_simd_latency;

    // SIMD bandwidth (in bytes per cycle)
    const uint32_t m_simd_bandwidth;

    // Next tick where SIMD is available
    Tick m_next_avail_tick;

    // Stats
    Stat::SampledValue<Tick> m_stat_blocked_ticks;
};

};  // namespace AstraSim
