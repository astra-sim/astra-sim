// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <astra-sim/common/Common.hh>
#include <astra-sim/common/SimObject.hh>
#include <astra-sim/common/stats/Statistics.hh>
#include <astra-sim/system/common/CallData.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/gpu/GpuOp.hh>
#include <astra-sim/system/gpu/MemoryEngine.hh>
#include <astra-sim/system/gpu/Workgroup.hh>

namespace AstraSim {

class GpuSys;
class TimingGPUModel;
class ComputeEngine;
class TimingWorkgroup;
class Wavefront;

class ComputeUnit : public Callable, SimObject {
  public:
    /** CallData for CU-specific event */
    class CU_Event_CallData : public CallData {
      public:
        CU_Event_CallData() : CallData() {}
        CU_Event_CallData(WFReqPtr _wfreq_p) : CallData(), wfreq_p(_wfreq_p) {}
        CU_Event_CallData(WFReqPtr _wfreq_p, uint64_t _vaddr)
            : CallData(),
              wfreq_p(_wfreq_p),
              vaddr(_vaddr) {}
        CU_Event_CallData(TimingWorkgroup* _wg_p) : CallData(), wg_p(_wg_p) {}
        ~CU_Event_CallData() override = default;
        WFReqPtr wfreq_p = nullptr;
        TimingWorkgroup* wg_p = nullptr;
        uint64_t vaddr = 0;
    };

    ComputeUnit(TimingGPUModel* gpu_model_p, GpuSys* sys_p, uint32_t id);
    ~ComputeUnit() override;

    // Call() for Callable
    void call(EventType type, CallData* data) override;

    // name() for SimObject
    const std::string& name() const override;

    // Return CU ID
    uint32_t getId() const;

    // curTick() for SimObject
    Tick curTick() const override;

    // Schedule a simulation event
    void scheduleEventInCycles(EventType event_type,
                               CallData* call_data_p,
                               uint64_t cycles);

    // Return max number of active WGs
    uint32_t getMaxWGs() const;

    // Return true if it's available to take in a new WG
    bool isAvailForNewWG() const;

    // Dispatch a new workgroup to this CU
    void dispatchWorkgroup(TimingWorkgroup* wg_p);

    // Determine dispatch to issue delay
    uint32_t getIssueLatencyForInst(GpuInst inst_type) const;

    // Dispatch next WF reqs from active wavefronts
    void dispatchNextWFReqs();

    // Issue WF req from the given active WF to the network
    void issueWFReq(WFReqPtr wfreq_p);

    // Wake up a WF req (i.e., callback from network).
    // vaddr is the specific aligned address that completed (for LSQ path).
    void wakeupWFReq(WFReqPtr wfreq_p, uint64_t vaddr = 0);

    // Complete a WF req (i.e., all ops have finished)
    void completeWFReq(WFReqPtr wfreq_p);

    // Return pointer to system
    GpuSys* getSysPtr() const;

    // Return pointer to GPU model
    TimingGPUModel* getGpuModelPtr() const;

    // Schedule an event to unstall a workgroup
    void schedWorkgroupUnstallEvent(TimingWorkgroup* wg_p);

    // Trace a new wavefront operation
    void traceWavefrontOpUpdate(Wavefront* wf_p) const;

  private:
    // Schedule an event to dispatch new WF requests in the next cycle
    void schedWFReqDispatchEvent();

    // Complete a wavefront (i.e., all its requests have finished)
    void completeWavefront(Wavefront* wf_p);

    // Finalize a wfreq whose LSQ entry is fully complete: collect stats
    // and call completeWFReq.
    void finalizeLSQEntry(WFReqPtr wfreq_p);

  private:
    TimingGPUModel* m_gpu_model_p;       // GPU model where this CU belongs to
    const uint32_t m_id;                 // CU ID
    const uint32_t m_max_wgs;            // Max number of active WGs
    const uint32_t m_wfreq_size_nbytes;  // Size of each WF req
    const uint32_t m_num_wf_groups;      // Number of wavefront groups
    const uint32_t m_nthreads_per_wf;    // Number of threads per wavefront
    const uint32_t m_max_outstanding_wfreqs;  // Max number of outstanding reqs
    const uint32_t m_max_wfreqs_dispatched_per_cycle;
    const uint32_t m_max_retry_threshold;  // Max retry threshold for a wfreq
    const uint32_t m_cu_issue_cycles;      // Latency in cycles to issue inst
    const bool m_use_random_wf_select;     // Select next WF randomly
    const bool m_use_mshr;                 // Enable load MSHR coalescing
    const std::string m_name;              // Name for logging
    ComputeEngine* m_compute_engine_p;     // Engine for handling compute ops

    // Set of active WGs
    std::unordered_set<TimingWorkgroup*> m_active_wg_set;

    // Wavefront groups, each of which corresponds to a SIMD unit
    std::vector<std::list<Wavefront*>> m_wf_groups;

    // Next WF group ID (for RR scheduling across WF groups)
    uint32_t m_next_wf_group_id;

    // Current number of outstanding LSQ entries (all usesLSQ insts).
    uint32_t m_num_outstanding_wfreqs;

    // True if an event has been already scheduled for dispatching new WF
    // requests in the next cycle
    bool m_wfreq_dispatch_scheduled;

    // Memory movement structures (LSQ, MSHR, ...).
    std::unique_ptr<MemoryEngine> m_mem_engine;

    // Stats
    Stat::Counter m_stat_num_wgs;
    std::unordered_map<GpuInst, Stat::SampledValue<Tick>> m_stat_wfreq_lat;
    Stat::Counter m_stat_num_blocked_net_ops;
    Stat::Counter m_stat_num_blocked_simd_ops;
    Stat::SampledValue<uint32_t> m_stat_sampled_wfreq_cnt;
    Stat::SampledValue<uint32_t> m_stat_coalescing_factor;
};

}  // namespace AstraSim
