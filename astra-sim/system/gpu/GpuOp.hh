// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace AstraSim {

class FunctionalGPUModel;
class Wavefront;
class WFReq;
class Workgroup;
typedef std::shared_ptr<WFReq> WFReqPtr;

/**
 * List of GPU instructions
 */
enum class GpuInst {
    /** Data movement instructions */
    Load,    // Load from src chunks (either remote or local)
    Store,   // Store to dst chunks (either remote or local)
    Reduce,  // Reduce (i.e., ALU op)
    GLdRed,  // Group load-reduce to switch (INC)
    GSt,     // Group store to switch (INC)
    /** Control instructions */
    SemAcquire,  // Semaphore acquire (remote wait or local semaphore acquire)
    SemRelease,  // Semaphore release (remote signal or local semaphore release)
    WaitCnt,     // For handling inter-instruction data dependency
    Invalid
};

inline std::string getGpuInstName(GpuInst inst) {
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

/**
 * GpuOp is the base class for all workgroup-level operation types. GpuOp holds
 * "static" information about a workgroup operation, which is determined by an
 * input plan (e.g., MSCCL++). For example, GpuOp specifies which buffer(s) the
 * op should access and which channel(s) a synchronization uses.
 *
 * A GpuOp describes a sequence of instructions to be applied to a set of data
 * elements (i.e., SIMD execution style). The number of data elements is
 * defined by the buffer chunk size and the size of each data element (e.g.,
 * 256B per wavefront). A sequence of instructions is specific to an operation
 * type. Each data element is processed by a single wavefront in a workgroup.
 */

class GpuOp {
  public:
    // Constructor and destructor
    GpuOp(const std::string& n, uint32_t unrolling, uint64_t niters);
    virtual ~GpuOp() = default;

    // Set the branch target operation pointer
    void setBranchTargetOpPtr(std::shared_ptr<GpuOp> br_tgt_op_ptr);

    // Set the next operation pointer
    void setNextOpPtr(std::shared_ptr<GpuOp> nxt_op_ptr);

    // Execute this operation for a given wavefront and data element index.
    // Return a sequence of wavefront requests generated from this execution.
    virtual std::vector<WFReqPtr> execute(Wavefront* wf_p,
                                          uint64_t eid) const = 0;

    // Functionally executes the operator, given the GPU to simulate it on.
    // Returns true if we completed executing this operation, or false if we
    // should try executing it again sometime later.
    virtual bool executeFunctional(FunctionalGPUModel* gpu,
                                   Workgroup* wg_p) = 0;

    /** Getters */
    const std::string& getName() const;
    std::shared_ptr<GpuOp> getNextOpPtr() const;
    std::shared_ptr<GpuOp> getBranchTargetOpPtr() const;
    uint32_t getUnrollingFactor() const;
    uint64_t getNumElems() const;
    uint64_t getNumElemsPerIter() const;
    uint64_t getNumIters() const;

    // Return string representation of this GpuOp
    virtual std::string to_string() const = 0;

  protected:
    // Init data element information
    void setOpSize(uint64_t data_nbytes,
                   uint32_t wfreq_nbytes,
                   uint32_t elem_nbytes,
                   uint64_t iter_nbytes);

    // Set instruction dependencies (e.g., for waitcnt)
    void setInstDependencies(const std::vector<WFReqPtr>& wfreq_lst) const;

  protected:
    // Operation name
    const std::string m_name;
    // Loop unrolling factor (i.e., number of instruction sequences that can be
    // generated at a time)
    const uint32_t m_unrolling;
    // Pointer to the next operation in the sequence (nullptr if this is the
    // last op in a workgroup)
    std::shared_ptr<GpuOp> m_nxt_op_ptr = nullptr;
    // Pointer to a branch target operation (if any)
    std::shared_ptr<GpuOp> m_br_tgt_op_ptr = nullptr;
    // Size of each data element in bytes
    uint32_t m_elem_nbytes = 0;
    // Size of each wavefront request in bytes
    uint32_t m_wfreq_nbytes = 0;
    // Total number of data elements
    uint64_t m_nelems = 0;
    // Number of data elements per iteration
    uint64_t m_nelems_per_iter = 0;
    // Number of iterations
    uint64_t m_niters = 0;
};

// Shared pointer to GpuOp
typedef std::shared_ptr<GpuOp> GpuOpPtr;

}  // namespace AstraSim
