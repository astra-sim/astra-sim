// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <memory>

#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/system/gpu/ComputeUnit.hh>
#include <astra-sim/system/gpu/Wavefront.hh>
#include <astra-sim/system/gpu/Workgroup.hh>
#include <astra-sim/system/gpu/msccl/MscclOps.hh>

using namespace AstraSim;

Wavefront::Wavefront(TimingWorkgroup* wg_p,
                     uint32_t wf_id,
                     uint32_t num_wfs,
                     GpuOpPtr first_op_p)
    : m_wg_p(wg_p),
      m_wf_id(wf_id),
      m_curr_op_p(first_op_p),  // always start from the first op
      m_curr_iter_idx(0) {      // always start from iteration 0
    // Sanity checks
    assert(m_wg_p);
    assert(m_curr_op_p);
    assert(m_wf_id < num_wfs);

    // Set the next element index for the current operation & iteration
    setElemIndices();

    // Trace the first op
    m_wg_p->getComputeUnitPtr()->traceWavefrontOpUpdate(this);
}

uint32_t Wavefront::getWfId() const {
    return m_wf_id;
}

GpuOpPtr Wavefront::getCurOpPtr() const {
    return m_curr_op_p;
}

int Wavefront::getWfGroupId() const {
    return m_wf_group_id;
}

void Wavefront::setWfGroupId(int wf_group_id) {
    assert(wf_group_id >= 0);
    if (m_wf_group_id >= 0) {
        THROW_ERR_NR("[Wavefront::setWfGroupId] Wavefront group ID is "
                     "already set: wf=%d, wf_group_id=%d\n",
                     m_wf_id, m_wf_group_id);
    }
    m_wf_group_id = wf_group_id;
}

TimingWorkgroup* Wavefront::getWorkgroupPtr() const {
    return m_wg_p;
}

uint64_t Wavefront::getCurrentIterationIdx() const {
    return m_curr_iter_idx;
}

bool Wavefront::hasMoreOps() const {
    return (m_curr_op_p != nullptr);
}

bool Wavefront::isCompleted() const {
    return !hasMoreOps() && (m_num_outstanding_wfreqs == 0);
}

bool Wavefront::isStalled() const {
    return m_stalled;
}

bool Wavefront::isOnWaitCnt() const {
    if (!m_rdy_wfreq_queue.empty()) {
        WFReqPtr wfreq_p = m_rdy_wfreq_queue.front();
        return (wfreq_p->inst == GpuInst::WaitCnt) && (wfreq_p->waitcnt > 0);
    }
    return false;
}

void Wavefront::stall() {
    if (m_stalled) {
        THROW_ERR_NR("[Wavefront::stall] Wavefront is already stalled: wf=%d\n",
                     m_wf_id);
    }
    if (!isNop() && !isBarrier()) {
        THROW_ERR_NR("[Wavefront::stall] Can only stall at Nop or Barrier "
                     "ops: wf=%d, op=%s\n",
                     m_wf_id, m_curr_op_p->getName().c_str());
    }
    if (!m_rdy_wfreq_queue.empty() || (m_num_outstanding_wfreqs > 0)) {
        THROW_ERR_NR("[Wavefront::stall] Cannot stall when there are "
                     "outstanding wfreqs: wf=%d, rdy_wfreq_queue_size=%d, "
                     "num_outstanding_wfreqs=%d\n",
                     m_wf_id, m_rdy_wfreq_queue.size(),
                     m_num_outstanding_wfreqs);
    }
    m_stalled = true;
}

void Wavefront::unstall() {
    if (!m_stalled) {
        THROW_ERR_NR("[Wavefront::unstall] Wavefront is not stalled: wf=%d\n",
                     m_wf_id);
    }
    if (!isNop() && !isBarrier()) {
        THROW_ERR_NR("[Wavefront::unstall] Can only unstall at Nop or Barrier "
                     "ops: wf=%d, op=%s\n",
                     m_wf_id, m_curr_op_p->getName().c_str());
    }
    if (!m_rdy_wfreq_queue.empty() || (m_num_outstanding_wfreqs > 0)) {
        THROW_ERR_NR("[Wavefront::unstall] Cannot unstall when there are "
                     "outstanding wfreqs: wf=%d, rdy_wfreq_queue_size=%d, "
                     "num_outstanding_wfreqs=%d\n",
                     m_wf_id, m_rdy_wfreq_queue.size(),
                     m_num_outstanding_wfreqs);
    }
    m_stalled = false;

    // Now, we're unstalled, so we advance to the next op
    advanceOp();
}

WFReqPtr Wavefront::peekNextReadyWFReq() {
    if (!hasMoreOps() || isStalled()) {
        return nullptr;
    }

    if (isNop() || isBarrier()) {
        return nullptr;
    }

    if (m_rdy_wfreq_queue.empty()) {
        dispatchWFReqs();
    }

    WFReqPtr wfreq_p =
        (m_rdy_wfreq_queue.empty()) ? nullptr : m_rdy_wfreq_queue.front();

    // Handle waitcnt instruction(s)
    while ((wfreq_p != nullptr) && (wfreq_p->inst == GpuInst::WaitCnt)) {
        if (wfreq_p->waitcnt > 0) {
            wfreq_p = nullptr;
        } else {
            m_rdy_wfreq_queue.pop();
            wfreq_p = (m_rdy_wfreq_queue.empty()) ? nullptr
                                                  : m_rdy_wfreq_queue.front();
        }
    }

    return wfreq_p;
}

WFReqPtr Wavefront::execute() {
    if (!hasMoreOps() || isStalled()) {
        // Return nullptr as there is no WF request to execute now
        return nullptr;
    }

    if (isNop()) {
        assert(m_rdy_wfreq_queue.empty());
        if (m_num_outstanding_wfreqs == 0) {
            // Call workgroup to execute this Nop when we have finished all
            // outstanding requests
            m_wg_p->execNopOp(this);
        }
        return nullptr;
    }

    if (isBarrier()) {
        auto barrier_op_p =
            std::dynamic_pointer_cast<Msccl::BarrierOp>(m_curr_op_p);
        assert(barrier_op_p);
        assert(m_rdy_wfreq_queue.empty());
        if (m_num_outstanding_wfreqs == 0) {
            // Call workgroup to execute this Barrier when we have finished all
            // outstanding requests
            m_wg_p->execBarrierOp(this, barrier_op_p->getBarrierId(),
                                  barrier_op_p->getNumWorkgroups());
        }
        return nullptr;
    }

    WFReqPtr wfreq_p = peekNextReadyWFReq();

    if (wfreq_p) {
        // If we have a non-waitcnt, pop it, increment number of outstanding
        // wfreqs, and return it.
        m_rdy_wfreq_queue.pop();
        m_num_outstanding_wfreqs++;
    }

    if (m_rdy_wfreq_queue.empty() && isEndOfIteration()) {
        // If there is no more ready wfreqs and we've reached the end of the
        // current iteration, we can advance to the next op now.
        advanceOp();
    }

    return wfreq_p;
}

void Wavefront::completeWFReq(WFReqPtr wfreq_p) {
    assert(wfreq_p);
    assert(wfreq_p->wf_p == this);
    assert(wfreq_p->status == WFReq::Status::Issued);
    assert(m_num_outstanding_wfreqs > 0);

    // Mark this wfreq as completed
    wfreq_p->status = WFReq::Status::Completed;

    // Decrement number of outstanding wfreqs
    m_num_outstanding_wfreqs--;

    // Notify waitcnt (if any) waiting on this request
    if (wfreq_p->waitcnt_wfreq_p) {
        assert(wfreq_p->waitcnt_wfreq_p->waitcnt > 0);
        wfreq_p->waitcnt_wfreq_p->waitcnt--;
    }
}

void Wavefront::dispatchWFReqs() {
    if (!m_rdy_wfreq_queue.empty()) {
        THROW_ERR_NR("[Wavefront::dispatchWFReqs] Ready wfreq queue is not "
                     "empty: wf=%d, size=%d\n",
                     m_wf_id, m_rdy_wfreq_queue.size());
        return;
    }

    if (isStalled()) {
        THROW_ERR_NR("[Wavefront::dispatchWFReqs] Cannot dispatch wfreqs when "
                     "stalled: wf=%d\n",
                     m_wf_id);
        return;
    }

    if (!hasMoreOps()) {
        // DPRINTFNR("[Wavefront::dispatchWFReqs] No more ops to dispatch reqs:
        // "
        //           "wf=%d\n", m_wf_id);
        return;
    }

    if (isNop() || isBarrier()) {
        // DPRINTFNR("[Wavefront::dispatchWFReqs] Nop and Barrier ops do not "
        //           "generate wfreqs: wf=%d\n", m_wf_id);
        return;
    }

    // Unroll multiple times until we reach the end of the current iteration or
    // the unrolling limit. All sequences have the same instruction sequence,
    // except on different data elements.
    uint32_t num_unrolls = m_curr_op_p->getUnrollingFactor();
    uint32_t num_wfreqs = 0;
    std::vector<std::vector<WFReqPtr>> new_seq_lst;
    new_seq_lst.reserve(num_unrolls);
    while (!isEndOfIteration() && (new_seq_lst.size() < num_unrolls)) {
        // DPRINTFNR("[Wavefront::dispatchWFReqs] Unrolling op %s for wf=%d, "
        //           "iter=%d, next_elem_idx=%d\n",
        //           m_curr_op_p->getName().c_str(), m_wf_id, m_curr_iter_idx,
        //           m_nxt_elem_idx);
        auto new_seq = m_curr_op_p->execute(this, m_nxt_elem_idx);
        new_seq_lst.push_back(new_seq);
        num_wfreqs += new_seq.size();
        // Advance to the next element
        advanceElement();
    }

    // Now, each sequence in new_seq_lst represents a sequence of WFReqs for a
    // single loop iteration. In reality, when a compiler unrolls multiple loop
    // iterations, it would try to interleave instructions from different
    // sequences so that we can maximize the number of instructions that can be
    // executed in parallel before hitting a waitcnt. For example,
    //
    // +------+------+--------+
    // | seq0 | seq1 | merged |
    // +------+------+--------+
    // | A0   | A1   | A0     |
    // | W0_0 | W1_0 | A1     |
    // | B0   | B1   | W0_0   |
    // | W0_1 | W1_1 | B0     |
    // | C0   | C1   | W1_0   |
    // |      |      | B1     |
    // |      |      | W0_1   |
    // |      |      | C0     |
    // |      |      | W1_1   |
    // |      |      | C1     |
    // +------+------+--------+

    std::vector<uint32_t> seq_idx_lst(new_seq_lst.size(), 0);
    uint32_t num_remaining_seqs = new_seq_lst.size();
    while (num_remaining_seqs > 0) {
        for (auto i = 0; i < new_seq_lst.size(); ++i) {
            auto& seq = new_seq_lst[i];
            auto& seq_idx = seq_idx_lst[i];
            if (seq_idx >= seq.size()) {
                // This sequence is done
                continue;
            }

            // Handle the first waitcnt instruction
            if (seq[seq_idx]->inst == GpuInst::WaitCnt) {
                m_rdy_wfreq_queue.push(seq[seq_idx]);
                seq_idx++;
            }

            // Keep pushing non-waitcnt instructions until we reach the next
            // waitcnt or the end of the sequence
            while ((seq_idx < seq.size()) &&
                   (seq[seq_idx]->inst != GpuInst::WaitCnt)) {
                m_rdy_wfreq_queue.push(seq[seq_idx]);
                seq_idx++;
            }

            if (seq_idx >= seq.size()) {
                // This sequence is done
                num_remaining_seqs--;
            }
        }
    }

    // Sanity check to make sure all new wfreqs are pushed to the ready queue
    assert(m_rdy_wfreq_queue.size() == num_wfreqs);
}

void Wavefront::advanceOp() {
    if (!hasMoreOps()) {
        THROW_ERR_NR("[Wavefront::advanceOp] Cannot advance op when there is "
                     "no more ops: wf=%d\n",
                     m_wf_id);
    }

    if (isStalled()) {
        THROW_ERR_NR("[Wavefront::advanceOp] Cannot advance op when stalled: "
                     "wf=%d, op=%s\n",
                     m_wf_id, m_curr_op_p->getName().c_str());
    }

    DPRINTFNR("[Wavefront::advanceOp] Advancing op for wf=%d, iter=%d, "
              "current_op=%s\n",
              m_wf_id, m_curr_iter_idx, m_curr_op_p->getName().c_str());

    // Current operation's info
    auto niters = m_curr_op_p->getNumIters();
    assert(m_curr_iter_idx < niters);
    auto nxt_op_p = m_curr_op_p->getNextOpPtr();
    auto br_tgt_op_p = m_curr_op_p->getBranchTargetOpPtr();

    if (br_tgt_op_p) {
        if (m_curr_iter_idx + 1 == niters) {
            // We're at the last iteration of a loop, so move to the next op
            // (after the loop) and reset the iteration to 0 as we're moving
            // out of the loop.
            m_curr_op_p = nxt_op_p;
            m_curr_iter_idx = 0;
        } else {
            // We're still in the loop, so jump to the branch target op and
            // increment the current iteration index
            m_curr_op_p = br_tgt_op_p;
            m_curr_iter_idx++;
        }
    } else {
        // Simply move to the next op in the same iteration
        m_curr_op_p = nxt_op_p;
    }

    DPRINTFNR("[Wavefront::advanceOp] New op for wf=%d, iter=%d, new_op=%s\n",
              m_wf_id, m_curr_iter_idx,
              (m_curr_op_p != nullptr) ? m_curr_op_p->getName().c_str()
                                       : "nullptr");

    // Reset the next element index for the new operation & iteration
    setElemIndices();

    // Trace this new op
    m_wg_p->getComputeUnitPtr()->traceWavefrontOpUpdate(this);
}

bool Wavefront::isNop() const {
    return (std::dynamic_pointer_cast<Msccl::NopOp>(m_curr_op_p) != nullptr);
}

bool Wavefront::isBarrier() const {
    return (std::dynamic_pointer_cast<Msccl::BarrierOp>(m_curr_op_p) !=
            nullptr);
}

uint32_t Wavefront::getNumOutstandingWFReqs() const {
    return m_num_outstanding_wfreqs;
}

void Wavefront::advanceElement() {
    if (!hasMoreOps()) {
        THROW_ERR_NR("[Wavefront::advanceOp] Cannot advance op when there is "
                     "no more ops: wf=%d\n",
                     m_wf_id);
    }

    if (isStalled()) {
        THROW_ERR_NR("[Wavefront::advanceOp] Cannot advance op when stalled: "
                     "wf=%d, op=%s\n",
                     m_wf_id, m_curr_op_p->getName().c_str());
    }

    if (isNop() || isBarrier()) {
        THROW_ERR_NR("[Wavefront::advanceElement] Cannot advance element "
                     "when at Nop or Barrier op: wf=%d, op=%s\n",
                     m_wf_id, m_curr_op_p->getName().c_str());
    }

    if (isEndOfIteration()) {
        THROW_ERR_NR("[Wavefront::advanceElement] Cannot advance element "
                     "beyond the end of iteration: wf=%d, op=%s, iter=%d, "
                     "next_elem_idx=%d\n",
                     m_wf_id, m_curr_op_p->getName().c_str(), m_curr_iter_idx,
                     m_nxt_elem_idx);
    }

    // Advance the next element index by the number of wavefronts in the
    // workgroup
    m_nxt_elem_idx += m_wg_p->getNumWavefronts();
    if (m_nxt_elem_idx > m_max_elem_idx) {
        // We've exceeded the max element index for the current iteration,
        // so clamp it to the max element index
        m_nxt_elem_idx = m_max_elem_idx;
    }
    // DPRINTFNR("[Wavefront::advanceElement] Advanced next_elem_idx for "
    //           "wf=%d, op=%s, iter=%d, nelems=%d, nelems_per_iter=%d, "
    //           "next_elem_idx=%d\n",
    //           m_wf_id, m_curr_op_p->getName().c_str(), m_curr_iter_idx,
    //           nelems, nelems_per_iter, m_nxt_elem_idx);
}

bool Wavefront::isEndOfIteration() const {
    if (!hasMoreOps()) {
        THROW_ERR_NR("[Wavefront::isEndOfIteration] Cannot check end of "
                     "iteration when there is no more ops: wf=%d\n",
                     m_wf_id);
    }

    if (isNop() || isBarrier()) {
        THROW_ERR_NR("[Wavefront::isEndOfIteration] Cannot check end of "
                     "iteration when at Nop or Barrier op: wf=%d, op=%s\n",
                     m_wf_id, m_curr_op_p->getName().c_str());
    }

    if (m_nxt_elem_idx > m_max_elem_idx) {
        THROW_ERR_NR("[Wavefront::isEndOfIteration] Next element index "
                     "exceeds max element index: wf=%d, op=%s, iter=%d, "
                     "next_elem_idx=%d, max_elem_idx=%d\n",
                     m_wf_id, m_curr_op_p->getName().c_str(), m_curr_iter_idx,
                     m_nxt_elem_idx, m_max_elem_idx);
    }

    return (m_nxt_elem_idx == m_max_elem_idx);
}

void Wavefront::setElemIndices() {
    if (!hasMoreOps() || isNop() || isBarrier()) {
        // No data element to be processed, so just reset all element indices
        // to 0s
        m_nxt_elem_idx = 0;
        m_min_elem_idx = 0;
        m_max_elem_idx = 0;
        return;
    }

    // Current operation's info
    auto niters = m_curr_op_p->getNumIters();
    auto nelems = m_curr_op_p->getNumElems();
    auto nelems_per_iter = m_curr_op_p->getNumElemsPerIter();
    auto num_wfs = m_wg_p->getNumWavefronts();

    // Sanity checks
    if (m_curr_iter_idx >= niters) {
        THROW_ERR_NR("[Wavefront::setElemIndices] Current iteration index "
                     "exceeds number of iterations: wf=%d, op=%s, "
                     "curr_iter_idx=%d, num_iters=%d\n",
                     m_wf_id, m_curr_op_p->getName().c_str(), m_curr_iter_idx,
                     niters);
    }

    if (m_wf_id < nelems_per_iter) {
        m_min_elem_idx = m_curr_iter_idx * nelems_per_iter + m_wf_id;
        if (m_min_elem_idx >= nelems) {
            // No data elements assigned to this wavefront in this iteration
            m_min_elem_idx = 0;
            m_nxt_elem_idx = 0;
            m_max_elem_idx = 0;
        } else {
            m_nxt_elem_idx = m_min_elem_idx;
            auto per_wf_nelems = (nelems_per_iter % num_wfs == 0)
                                     ? (nelems_per_iter / num_wfs)
                                     : (nelems_per_iter / num_wfs + 1);
            m_max_elem_idx = m_min_elem_idx + per_wf_nelems * num_wfs;
            // Clamp max element index to the total number of elements
            if (m_max_elem_idx > nelems) {
                m_max_elem_idx = nelems;
            }
        }
    } else {
        // No data elements assigned to this wavefront in this iteration
        m_min_elem_idx = 0;
        m_nxt_elem_idx = 0;
        m_max_elem_idx = 0;
    }
}

std::string Wavefront::to_string() const {
    std::stringstream ss;
    ss << "[WF] wf=" << m_wf_id;
    ss << ",wg=(" << m_wg_p->to_string() << ")";
    ss << ",curr_op=";
    if (m_curr_op_p) {
        ss << m_curr_op_p->getName();
    } else {
        ss << "null";
    }
    ss << ",curr_iter=" << m_curr_iter_idx;
    ss << ",nxt_elem_idx=" << m_nxt_elem_idx;
    ss << ",min_elem_idx=" << m_min_elem_idx;
    ss << ",max_elem_idx=" << m_max_elem_idx;
    ss << ",num_outstanding_wfreqs=" << m_num_outstanding_wfreqs;
    ss << ",stalled=" << (m_stalled ? "true" : "false");
    return ss.str();
}
