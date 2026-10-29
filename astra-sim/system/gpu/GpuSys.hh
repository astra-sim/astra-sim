// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/common/Sys.hh>
#include <astra-sim/system/gpu/Memory.hh>

namespace AstraSim {

class GPUModel;

/**
 * The GPU-driven system model: dispatches MSCCL kernel nodes to a GPUModel
 * and owns the compute-unit configuration that model reads.
 */
class GpuSys : public Sys {
  public:
    GpuSys(int id,
           std::string workload_configuration,
           std::string comm_group_configuration,
           std::string system_configuration,
           AstraRemoteMemoryAPI* remote_mem,
           AstraNetworkAPI* comm_NI,
           timespec_t clock_period,
           uint64_t comm_size = 0,
           std::string custom_comm_plan_map_filename = "");
    ~GpuSys() override;

    // Get clock period in ticks
    Tick getClockPeriodInTicks() const;

    void registerEventInCycles(Callable* callable,
                               EventType event,
                               CallData* callData,
                               uint64_t delta_cycles);

    //----------------------------------------------------------------------------
    // GPU params
    //----------------------------------------------------------------------------
    // Number of CUs in GPU
    uint32_t num_cus;
    // Number of WF groups (i.e., each group corresponds to one SIMD) in CU
    uint32_t num_wf_groups_per_cu;
    // Max number of WGs per CU
    uint32_t max_wgs_per_cu;
    // Max number of outstanding requests per compute unit
    uint32_t max_outstanding_wfreqs_per_cu;
    // Max number of requests that can be dispatched per cycle in CU
    uint32_t max_wfreqs_dispatched_per_cycle;
    // Number of threads per wavefront
    uint32_t num_threads_per_wf;
    // Size of data on which each wavefront request works on (in bytes)
    uint32_t wfreq_size_nbytes;
    // SIMD latency (in cycles)
    uint32_t simd_latency;
    // SIMD bandwidth (in bytes per cycle)
    uint32_t simd_bandwidth;
    // Control how much loop unrolling is per MSCCL loop
    // This influences how many consecutive iterations in a wavefront can be
    // executed in parallel.
    uint32_t msccl_loop_unrolling_factor;
    // Latency (in cycles) to issue an instruction from CU
    uint32_t cu_issue_cycles;
    // Latency (in cycles) before retrying a failed wait
    uint32_t wait_retry_cycles;
    // True if using random WF selection per WF group
    bool use_random_wf_select;
    // True if load MSHR coalescing is enabled
    bool use_mshr;
    // True if we use monitor wait for waiting on semaphores
    bool use_mwait;
    // True if we want to do a timing simulation, false if we want to do a
    // functional simulation
    bool timing_simulation;
    // The representation mode for the functional memory.
    enum Memory::MemoryMode memory_mode;
    // Load-store queue depth (max entries tracked concurrently per CU)
    uint32_t lsq_depth;
    // Memory request size used by the load-store queue for address alignment
    // (cache-line granularity). Independent of wfreq_size_nbytes so that e.g.
    // a 512B wfreq can be split into 256B-aligned cache-line requests.
    uint32_t lsq_req_size_nbytes;

    GPUModel* gpu_model;

  protected:
    DataSet* issueCollective(WrapperNode& node,
                             const std::vector<bool>& involved_dim,
                             CommunicatorGroup* comm_group) override;
    bool claimEvent(EventType event, BasicEventHandlerData* ehd) override;

  private:
    // Opens system_configuration a second time and parses this class's own
    // key slice; see Sys::initializeSys for why this cannot be a virtual
    // call from the base constructor.
    void parseGpuConfig(const std::string& filename);

    void dispatchReadyNode(WrapperNode& node);

    // Clock period in ps
    timespec_t m_clock_period;
};

}  // namespace AstraSim
