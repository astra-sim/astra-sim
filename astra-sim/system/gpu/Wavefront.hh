// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

/**
 * Modeling a single wavefront in CU
 */

#pragma once

#include <queue>

#include <astra-sim/system/gpu/GpuOp.hh>
#include <astra-sim/system/gpu/WavefrontReq.hh>
#include <astra-sim/system/gpu/Workgroup.hh>

namespace AstraSim {

class TimingWorkgroup;

/**
 * A wavefront represents a collection of threads executing in lock-step within
 * a workgroup. Each wavefront works on a data element at a time. The size of a
 * data element is defined as wavefront request size (i.e., consecutive bytes
 * processed by threads of the wavefront).
 *
 * Wavefront captures the dynamic execution state which is defined by
 * (1) the current operation,
 * (2) the current data element index within the operation, and
 * (3) the current iteration index of a pipeline loop
 */

class Wavefront {
  public:
    Wavefront(TimingWorkgroup* wg_p,
              uint32_t wf_id,
              uint32_t num_wfs,
              GpuOpPtr first_op_p);
    ~Wavefront() = default;

    /** Getters */
    uint32_t getWfId() const;
    GpuOpPtr getCurOpPtr() const;
    int getWfGroupId() const;
    TimingWorkgroup* getWorkgroupPtr() const;
    uint64_t getCurrentIterationIdx() const;

    /** Setters */
    void setWfGroupId(int wf_group_id);

    // Return true if the wavefront has more operations to execute
    bool hasMoreOps() const;

    // Return true if the wavefront has completed all operations and all
    // outstanding requests
    bool isCompleted() const;

    // Return true if the wavefront is stalled (e.g., waiting on a syncthreads)
    bool isStalled() const;

    // Return true if the wavefront is waiting on a pending waitcnt
    bool isOnWaitCnt() const;

    // Stall this wavefront (e.g., when hitting a syncthreads)
    void stall();

    // Unstall this wavefront (after completing a syncthreads)
    void unstall();

    // Dispatch new WF requests
    void dispatchWFReqs();

    // Pop next ready wfreq
    WFReqPtr peekNextReadyWFReq();
    WFReqPtr execute();

    // Complete a previously issued wfreq
    void completeWFReq(WFReqPtr wfreq_p);

    // Return true if the current operation is a NopOp
    bool isNop() const;

    // Return true if the current operation is a barrier
    bool isBarrier() const;

    // Return the number of outstanding WF requests
    uint32_t getNumOutstandingWFReqs() const;

    // Return a string representation of this wavefront
    std::string to_string() const;

  private:
    // Advance the wavefront to the next operation. If there is no more
    // operation to advance to, this function does nothing.
    void advanceOp();

    // Advance the wavefront to the next data element within the current
    // iteration of the current operation. If there is no more data element
    // to advance to, this function does nothing.
    void advanceElement();

    // Set the min, max and next element indices based on the wavefront ID,
    // current op, and current iteration index at the beginning of a wavefront
    // execution and when we move to a new operation or iteration
    void setElemIndices();

    // Return true if we've reached the end of the current iteration of the
    // current operation
    bool isEndOfIteration() const;

  private:
    TimingWorkgroup* m_wg_p;  // pointer to the owning workgroup
    const uint32_t m_wf_id;   // wavefront ID within a workgroup
    GpuOpPtr m_curr_op_p;     // pointer to the current operation being executed
    uint64_t m_nxt_elem_idx;  // next element index in the current op
    uint64_t m_min_elem_idx;  // min element index in the current iteration
    uint64_t m_max_elem_idx;  // max element index in the current iteration
    uint64_t m_curr_iter_idx;                // current iteration index
    bool m_stalled = false;                  // true if this WF is stalled
    std::queue<WFReqPtr> m_rdy_wfreq_queue;  // queue of ready WFReqs
    uint32_t m_num_outstanding_wfreqs = 0;   // number of outstanding WFReqs
    int m_wf_group_id = -1;  // wavefront group ID (for CU scheduling)
};

}  // namespace AstraSim
