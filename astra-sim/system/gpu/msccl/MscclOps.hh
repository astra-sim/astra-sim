// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <vector>

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/gpu/GpuOp.hh>
#include <astra-sim/system/gpu/Workgroup.hh>
#include <astra-sim/system/gpu/msccl/Buffer.hh>
#include <astra-sim/system/gpu/msccl/Semaphore.hh>

namespace AstraSim {
namespace Msccl {

/**
 * Nop Operation: does syncthread
 */
class NopOp : public GpuOp {
  public:
    NopOp(uint64_t niters);
    ~NopOp() override = default;
    std::vector<WFReqPtr> execute(Wavefront* wf_p, uint64_t eid) const override;
    bool executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) override;
    std::string to_string() const override;
};

/**
 * Barrier Operation
 */
class BarrierOp : public GpuOp {
  public:
    BarrierOp(uint32_t barrier_id, uint32_t num_wgs, uint64_t niters);
    ~BarrierOp() override = default;
    uint32_t getBarrierId() const;
    uint32_t getNumWorkgroups() const;
    std::vector<WFReqPtr> execute(Wavefront* wf_p, uint64_t eid) const override;
    bool executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) override;
    std::string to_string() const override;

  private:
    uint32_t m_barrier_id;
    uint32_t m_num_wgs;  // Number of WGs waiting on this barrier
};

/**
 * Semaphore Acquire Operation: SemAcquire -> WaitCnt
 */
class SemAcquireOp : public GpuOp {
  public:
    SemAcquireOp(const std::string& op_name,
                 const std::vector<Semaphore>& sem_lst,
                 BufferType buffer_type,
                 uint32_t wfreq_nbytes,
                 uint64_t niters);
    SemAcquireOp(const std::string& op_name,
                 const std::vector<uint64_t>& sem_lst,
                 uint32_t wfreq_nbytes,
                 uint64_t niters);
    ~SemAcquireOp() override = default;
    std::vector<WFReqPtr> execute(Wavefront* wf_p, uint64_t eid) const override;
    bool executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) override;
    std::string to_string() const override;

  private:
    const std::vector<uint64_t> m_semaphores;
};

/**
 * Semaphore Release Operation: SemRelease -> WaitCnt
 */
class SemReleaseOp : public GpuOp {
  public:
    SemReleaseOp(const std::string& op_name,
                 const std::vector<Semaphore>& sem_lst,
                 BufferType buffer_type,
                 uint32_t wfreq_nbytes,
                 uint64_t niters);
    SemReleaseOp(const std::string& op_name,
                 const std::vector<uint64_t>& sem_lst,
                 uint32_t wfreq_nbytes,
                 uint64_t niters);
    ~SemReleaseOp() override = default;
    std::vector<WFReqPtr> execute(Wavefront* wf_p, uint64_t eid) const override;
    bool executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) override;
    std::string to_string() const override;

  private:
    const std::vector<uint64_t> m_semaphores;
};

/**
 * Load Operation: moving data from src buffer chunks to registers
 */
class LoadOp : public GpuOp {
  public:
    LoadOp(const std::vector<BufferChunk>& src,
           uint32_t unrolling,
           uint32_t wfreq_nbytes,
           uint64_t niters,
           uint64_t iter_nbytes);
    ~LoadOp() override = default;
    std::vector<WFReqPtr> execute(Wavefront* wf_p, uint64_t eid) const override;
    bool executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) override;
    std::string to_string() const override;

  private:
    std::vector<BufferChunk> m_src_chunks;
};

/**
 * Store Operation: moving data from registers to dst buffer chunks
 */
class StoreOp : public GpuOp {
  public:
    StoreOp(const std::vector<BufferChunk>& dst,
            uint32_t unrolling,
            uint32_t wfreq_nbytes,
            uint64_t niters,
            uint64_t iter_nbytes);
    ~StoreOp() override = default;
    std::vector<WFReqPtr> execute(Wavefront* wf_p, uint64_t eid) const override;
    bool executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) override;
    std::string to_string() const override;

  private:
    std::vector<BufferChunk> m_dst_chunks;
};

/**
 * MemCpy Operation: moving data between src and dst buffer chunks
 */
class MemCpyOp : public GpuOp {
  public:
    MemCpyOp(const std::string& op_name,
             const std::vector<BufferChunk>& src,
             const std::vector<BufferChunk>& dst,
             uint32_t unrolling,
             uint32_t wfreq_nbytes,
             uint64_t niters,
             uint64_t iter_nbytes);
    ~MemCpyOp() override = default;

    // Initializes things both the Simple and LL/LL128 protocols require.
    void init(uint64_t wfreq_nbytes,
              uint64_t data_in_wfreq,
              uint64_t iter_nbytes);

    std::vector<WFReqPtr> execute(Wavefront* wf_p, uint64_t eid) const override;
    bool executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) override;
    virtual std::string to_string() const override;

  protected:
    std::vector<BufferChunk> m_src_chunks;
    std::vector<BufferChunk> m_dst_chunks;
};

/**
 * MemCpy variant for packet plans, includes semaphores for each chunk.
 */
class MemCpyLLOp : public MemCpyOp {
  public:
    MemCpyLLOp(const std::string& op_name,
               const std::vector<BufferChunk>& src,
               const std::vector<BufferChunk>& dst,
               uint32_t unrolling,
               uint32_t wfreq_nbytes,
               uint64_t niters,
               uint64_t iter_nbytes);
    ~MemCpyLLOp() override = default;

    bool executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) override;
    std::string to_string() const override;

  private:
    std::unique_ptr<SemAcquireOp> m_sem_acquire;  // from[Scratch], to[In|Out]
    std::unique_ptr<SemReleaseOp> m_sem_release;  // from[In|Out],  to[Scratch]
};

/**
 * Reduce Operation: Load(s) -> WaitCnt -> Reduce -> Store(s)
 */
class ReduceOp : public GpuOp {
  public:
    ReduceOp(const std::string& op_name,
             const std::vector<BufferChunk>& src,
             const std::vector<BufferChunk>& dst,
             uint32_t unrolling,
             uint32_t wfreq_nbytes,
             uint64_t niters,
             uint64_t iter_nbytes);
    ~ReduceOp() override = default;
    std::vector<WFReqPtr> execute(Wavefront* wf_p, uint64_t eid) const override;
    bool executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) override;
    std::string to_string() const override;

  private:
    std::vector<BufferChunk> m_src_chunks;
    std::vector<BufferChunk> m_dst_chunks;
};

}  // namespace Msccl
}  // namespace AstraSim
