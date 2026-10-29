// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>
#include <cmath>

#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/system/gpu/GpuOp.hh>
#include <astra-sim/system/gpu/WavefrontReq.hh>

using namespace AstraSim;

GpuOp::GpuOp(const std::string& n, uint32_t unrolling, uint64_t niters)
    : m_name(n),
      m_unrolling(unrolling),
      m_niters(niters) {
    assert(m_unrolling > 0);
    assert(m_niters > 0);
}

void GpuOp::setOpSize(uint64_t data_nbytes,
                      uint32_t wfreq_nbytes,
                      uint32_t elem_nbytes,
                      uint64_t iter_nbytes) {
    assert(m_niters > 0);
    assert(data_nbytes > 0);
    assert(wfreq_nbytes > 0);
    assert(elem_nbytes > 0);
    if (iter_nbytes == 0) {
        // If iter_nbytes is not specified, set it to data_nbytes
        iter_nbytes = data_nbytes;
    }

    // data_nbytes may not be a multiple of wfreq_nbytes, in which case, the
    // last wavefront request will be partial

    // Sanity check for the consistency of the number of iterations
    auto _niters = (data_nbytes % iter_nbytes == 0)
                       ? data_nbytes / iter_nbytes
                       : (data_nbytes / iter_nbytes + 1);
    if (_niters != m_niters) {
        THROW_ERR_NR("[GpuOp::setOpSize] Inconsistent number of iterations "
                     "(%d != %d) calculated from data_nbytes (%d bytes) and "
                     "iter_nbytes (%d bytes) in op %s\n",
                     m_niters, _niters, data_nbytes, iter_nbytes,
                     m_name.c_str());
    }

    // Set size of each data element
    m_elem_nbytes = elem_nbytes;
    m_wfreq_nbytes = wfreq_nbytes;

    // Get the total number of data elements
    m_nelems = static_cast<uint64_t>(
        std::ceil(static_cast<double>(data_nbytes) / m_elem_nbytes));

    // Get the number of data elements per iteration
    m_nelems_per_iter = static_cast<uint64_t>(
        std::ceil(static_cast<double>(iter_nbytes) / m_elem_nbytes));

    DPRINTFNR("[GpuOp::setOpSize] Op %s: data_nbytes=%d, wfreq_nbytes=%d, "
              "elem_nbytes=%d, nelems=%d, nelems_per_iter=%d, niters=%d\n",
              m_name.c_str(), data_nbytes, wfreq_nbytes, m_elem_nbytes,
              m_nelems, m_nelems_per_iter, m_niters);
}

void GpuOp::setBranchTargetOpPtr(std::shared_ptr<GpuOp> br_tgt_op_ptr) {
    m_br_tgt_op_ptr = br_tgt_op_ptr;
}

void GpuOp::setNextOpPtr(std::shared_ptr<GpuOp> nxt_op_ptr) {
    m_nxt_op_ptr = nxt_op_ptr;
}

const std::string& GpuOp::getName() const {
    return m_name;
}

std::shared_ptr<GpuOp> GpuOp::getNextOpPtr() const {
    return m_nxt_op_ptr;
}

std::shared_ptr<GpuOp> GpuOp::getBranchTargetOpPtr() const {
    return m_br_tgt_op_ptr;
}

uint32_t GpuOp::getUnrollingFactor() const {
    return m_unrolling;
}

uint64_t GpuOp::getNumElems() const {
    return m_nelems;
}

uint64_t GpuOp::getNumElemsPerIter() const {
    return m_nelems_per_iter;
}

uint64_t GpuOp::getNumIters() const {
    return m_niters;
}

void GpuOp::setInstDependencies(const std::vector<WFReqPtr>& wfreq_lst) const {
    assert(!wfreq_lst.empty());

    // Set up waitcnt dependency within individual sequences. For example, in
    // sequence A -> B -> waitcnt, the waitcnt depends on both A and B.
    WFReqPtr last_waitcnt_p = nullptr;
    for (auto it = wfreq_lst.rbegin(); it != wfreq_lst.rend(); ++it) {
        if ((*it)->inst == GpuInst::WaitCnt) {
            // Update last waitcnt pointer
            last_waitcnt_p = *it;
        } else if (last_waitcnt_p != nullptr) {
            // If there is a waitcnt after this instruction, set up the
            // dependency between the current inst and the last waitcnt, and
            // increment the number of instructions the waitcnt is waiting for.
            (*it)->waitcnt_wfreq_p = last_waitcnt_p;
            last_waitcnt_p->waitcnt++;
        }
    }
}

std::string getGpuInstName(GpuInst inst) {
    switch (inst) {
    case GpuInst::Load:
        return "Load";
    case GpuInst::Store:
        return "Store";
    case GpuInst::Reduce:
        return "Reduce";
    case GpuInst::GLdRed:
        return "GLdRed";
    case GpuInst::GSt:
        return "GSt";
    case GpuInst::SemAcquire:
        return "SemAcquire";
    case GpuInst::SemRelease:
        return "SemRelease";
    case GpuInst::WaitCnt:
        return "WaitCnt";
    default:
        return "Invalid";
    }
}
