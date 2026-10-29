// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>
#include <string>

#include <astra-sim/common/logging/EventTracer.hh>
#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/system/gpu/ComputeUnit.hh>
#include <astra-sim/system/gpu/FunctionalGPUModel.hh>
#include <astra-sim/system/gpu/GPUModel.hh>
#include <astra-sim/system/gpu/GpuSys.hh>
#include <astra-sim/system/gpu/Wavefront.hh>
#include <astra-sim/system/gpu/msccl/MscclOps.hh>

using namespace AstraSim;
using namespace AstraSim::Msccl;

/**
 * NopOp
 */
NopOp::NopOp(uint64_t niters) : GpuOp("nop", 1, niters) {}

std::vector<WFReqPtr> NopOp::execute(Wavefront* wf_p, uint64_t eid) const {
    assert(wf_p);
    THROW_ERR_NR("[Msccl::NopOp] NopOp is not executable: wf=%d, eid=%d\n",
                 wf_p->getWfId(), eid);
    return {};
}

bool NopOp::executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) {
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const GpuOp* op) {
            os << "NopOp::executeFunctional -> " << op->to_string();
        },
        this);

    return true;
}

std::string NopOp::to_string() const {
    std::stringstream ss;
    ss << "[NopOp] niters=" << m_niters;
    return ss.str();
}

/**
 * BarrierOp
 */
BarrierOp::BarrierOp(uint32_t barrier_id, uint32_t num_wgs, uint64_t niters)
    : GpuOp("barrier", 1, niters),
      m_barrier_id(barrier_id),
      m_num_wgs(num_wgs) {
    assert(num_wgs > 0);
}

uint32_t BarrierOp::getBarrierId() const {
    return m_barrier_id;
}

uint32_t BarrierOp::getNumWorkgroups() const {
    return m_num_wgs;
}

std::vector<WFReqPtr> BarrierOp::execute(Wavefront* wf_p, uint64_t eid) const {
    assert(wf_p);
    THROW_ERR_NR("[Msccl::BarrierOp] BarrierOp is not executable: "
                 "wf=%d, eid=%d\n",
                 wf_p->getWfId(), eid);
    return {};
}

bool BarrierOp::executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) {
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const GpuOp* op) {
            os << "BarrierOp::executeFunctional -> " << op->to_string();
        },
        this);

    THROW_ERR_NR("[Msccl::BarrierOp] BarrierOp::executeFunctional is not yet "
                 "implemented: %s\n",
                 to_string());

    return true;
}

std::string BarrierOp::to_string() const {
    std::stringstream ss;
    ss << "[BarrierOp] barrier_id=" << m_barrier_id << ", num_wgs=" << m_num_wgs
       << ", niters=" << m_niters;
    return ss.str();
}

/**
 * SemAcquireOp
 */

static std::vector<uint64_t> semaphoreAddresses(
    const std::vector<Semaphore>& sem_lst, BufferType buffer_type) {
    std::vector<uint64_t> result;
    result.reserve(sem_lst.size());
    for (const Semaphore& sem : sem_lst) {
        result.push_back(GPUModel::getSemAddr(sem.gpu_id, sem.connected_gpu_id,
                                              sem.sem_id, buffer_type));
    }
    return result;
}

SemAcquireOp::SemAcquireOp(const std::string& op_name,
                           const std::vector<Semaphore>& sem_lst,
                           BufferType buffer_type,
                           uint32_t wfreq_nbytes,
                           uint64_t niters)
    : SemAcquireOp(op_name,
                   semaphoreAddresses(sem_lst, buffer_type),
                   wfreq_nbytes,
                   niters) {}

SemAcquireOp::SemAcquireOp(const std::string& op_name,
                           const std::vector<uint64_t>& sem_lst,
                           uint32_t wfreq_nbytes,
                           uint64_t niters)
    : GpuOp(op_name, 1, niters),
      m_semaphores(sem_lst) {
    assert(!m_semaphores.empty());
    // Init data element info - Just a single semaphore data element per
    // iteration
    setOpSize(wfreq_nbytes * niters,  // total data size in bytes
              wfreq_nbytes,           // size of each wavefront request in bytes
              wfreq_nbytes,   // size of each data element per wavefront request
              wfreq_nbytes);  // data size of each iteration
}

std::vector<WFReqPtr> SemAcquireOp::execute(Wavefront* wf_p,
                                            uint64_t eid) const {
    assert(wf_p);
    assert(eid < m_nelems);
    if (wf_p->getWfId() != 0) {
        THROW_ERR_NR("[Msccl::SemAcquireOp] Semaphore acquire is only "
                     "executed by WF0 in a workgroup: wf=%d, eid=%d\n",
                     wf_p->getWfId(), eid);
    }
    std::vector<WFReqPtr> wfreq_lst;
    // SemAcquire instructions
    for (const uint64_t& sem_vaddr : m_semaphores) {
        wfreq_lst.emplace_back(std::make_shared<WFReq>(
            wf_p, GpuInst::SemAcquire, sem_vaddr, m_wfreq_nbytes));
    }
    // WaitCnt instruction - wait for the acquire to complete before this
    // thread moves on to the next op
    wfreq_lst.emplace_back(
        std::make_shared<WFReq>(wf_p, GpuInst::WaitCnt, 0, m_wfreq_nbytes));

    // Set instruction dependencies
    setInstDependencies(wfreq_lst);
    return wfreq_lst;
}

bool SemAcquireOp::executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) {
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const GpuOp* op) {
            os << "SemAcquireOp::executeFunctional -> " << op->to_string();
        },
        this);

    // If all of the semaphores are not registered, then that means this is our
    // first time executing this GpuOp.
    unsigned num_no_register = 0;
    for (const uint64_t& sem_address : m_semaphores) {
        assert(gpu->getId() == GPUModel::getGpuIdFromAddr(sem_address));
        if (gpu->peekSemaphore(sem_address) ==
            SemaphorePeekStatus::NoRegister) {
            num_no_register++;
        }
    }

    // We can't have the semaphores be partially registered; this implies
    // semaphores are shared between SemAcquireOp's, which shouldn't happen.
    assert(num_no_register == 0 || num_no_register == m_semaphores.size());

    // Register all semaphores at once.
    if (num_no_register == m_semaphores.size()) {
        for (const uint64_t& sem_address : m_semaphores) {
            gpu->registerAcquireSemaphore(
                sem_address,
                GPUModel::CallbackFunc(
                    &Sys::handleEvent,
                    new WorkgroupExecHandlerData(gpu->getSysPtr()->id, wg_p)));
        }
    }

    // Check if we can acquire each semaphore. Only proceed if all semaphores
    // can be acquired at once.
    for (const uint64_t& sem_address : m_semaphores) {
        if (gpu->peekSemaphore(sem_address) ==
            SemaphorePeekStatus::RegisterNoAcquire) {
            // The semaphore has been registered before (see above), but we
            // cannot acquire it. Do not acquire any semaphore.
            return false;
        }
    }

    // If we made it to this point, then all semaphores can be acquired. Acquire
    // them.
    for (const uint64_t& sem_address : m_semaphores) {
        assert(gpu->peekSemaphore(sem_address) ==
               SemaphorePeekStatus::RegisterYesAcquire);
        bool result = gpu->acquireSemaphore(sem_address);
        assert(result);
    }

    // All acquires passed; free to proceed to next instruction.
    return true;
}

static std::string semAddrToString(uint64_t addr) {
    std::stringstream ss;
    ss << "sem[addr=" << addr << ", gpu=" << GPUModel::getGpuIdFromAddr(addr)
       << ", buf_type="
       << static_cast<uint64_t>(GPUModel::getBufTypeFromAddr(addr))
       << ", offset=" << GPUModel::getBufByteOffFromAddr(addr) << "]";
    return ss.str();
}

std::string SemAcquireOp::to_string() const {
    std::stringstream ss;
    ss << "[SemAcquireOp] semaphores=[";
    for (size_t i = 0; i < m_semaphores.size(); ++i) {
        const uint64_t& sem_addr = m_semaphores[i];
        ss << semAddrToString(sem_addr);
        if (i != (m_semaphores.size() - 1)) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

/**
 * SemReleaseOp
 */
SemReleaseOp::SemReleaseOp(const std::string& op_name,
                           const std::vector<Semaphore>& sem_lst,
                           BufferType buffer_type,
                           uint32_t wfreq_nbytes,
                           uint64_t niters)
    : SemReleaseOp(op_name,
                   semaphoreAddresses(sem_lst, buffer_type),
                   wfreq_nbytes,
                   niters) {}

SemReleaseOp::SemReleaseOp(const std::string& op_name,
                           const std::vector<uint64_t>& sem_lst,
                           uint32_t wfreq_nbytes,
                           uint64_t niters)
    : GpuOp(op_name, 1, niters),
      m_semaphores(sem_lst) {
    assert(!m_semaphores.empty());
    // Init data element info - Just a single semaphore data element per
    // iteration
    setOpSize(wfreq_nbytes * niters,  // total data size in bytes
              wfreq_nbytes,           // size of each wavefront request in bytes
              wfreq_nbytes,
              wfreq_nbytes);  // data size of each iteration
}

std::vector<WFReqPtr> SemReleaseOp::execute(Wavefront* wf_p,
                                            uint64_t eid) const {
    assert(wf_p);
    assert(eid < m_nelems);
    if (wf_p->getWfId() != 0) {
        THROW_ERR_NR("[Msccl::SemReleaseOp] Semaphore release is only "
                     "executed by WF0 in a workgroup: wf=%d, eid=%d\n",
                     wf_p->getWfId(), eid);
    }
    std::vector<WFReqPtr> wfreq_lst;
    // SemRelease instructions
    for (const uint64_t& sem_vaddr : m_semaphores) {
        wfreq_lst.emplace_back(std::make_shared<WFReq>(
            wf_p, GpuInst::SemRelease, sem_vaddr, m_wfreq_nbytes));
    }
    // No need to wait for semaphore release. We can advance to the next op
    // immediately after executing the release.

    // Set instruction dependencies
    setInstDependencies(wfreq_lst);
    return wfreq_lst;
}

bool SemReleaseOp::executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) {
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const GpuOp* op) {
            os << "SemReleaseOp::executeFunctional -> " << op->to_string();
        },
        this);

    for (const uint64_t& sem_address : m_semaphores) {
        // Releasing a semaphore could lead to other instructions executing.
        // Have the GPU handle this.
        FunctionalGPUModel* sem_gpu = FunctionalGPUModel::getGpuModelPtr(
            GPUModel::getGpuIdFromAddr(sem_address));
        bool result = sem_gpu->releaseSemaphore(sem_address);
        assert(result);
    }

    // Immediately run the next instruction. Semaphore releases never result in
    // stalls.
    return true;
}

std::string SemReleaseOp::to_string() const {
    std::stringstream ss;
    ss << "[SemReleaseOp] semaphores=[";
    for (size_t i = 0; i < m_semaphores.size(); ++i) {
        const uint64_t& sem_addr = m_semaphores[i];
        ss << semAddrToString(sem_addr);
        if (i != (m_semaphores.size() - 1)) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

/**
 * LoadOp
 */
LoadOp::LoadOp(const std::vector<BufferChunk>& src,
               uint32_t unrolling,
               uint32_t wfreq_nbytes,
               uint64_t niters,
               uint64_t iter_nbytes)
    : GpuOp("load", unrolling, niters),
      m_src_chunks(src) {
    // Sanity checks
    assert(wfreq_nbytes > 0);
    if (m_src_chunks.empty()) {
        THROW_ERR_NR("LoadOp must have at least one source chunk (%d)\n",
                     m_src_chunks.size());
    }
    for (const auto& chunk : m_src_chunks) {
        if (chunk.nbytes != m_src_chunks[0].nbytes) {
            THROW_ERR_NR("LoadOp source chunks must have the same size: "
                         "%d != %d\n",
                         chunk.nbytes, m_src_chunks[0].nbytes);
        }
    }
    // Init data element info
    setOpSize(m_src_chunks[0].nbytes, wfreq_nbytes, wfreq_nbytes, iter_nbytes);
}

std::vector<WFReqPtr> LoadOp::execute(Wavefront* wf_p, uint64_t eid) const {
    assert(wf_p);
    assert(eid < m_nelems);
    std::vector<WFReqPtr> wfreq_lst;
    for (const auto& chunk : m_src_chunks) {
        // Compute the virtual address for the element index in the chunk
        uint64_t buf_byte_off = chunk.byte_offset + m_elem_nbytes * eid;
        assert(buf_byte_off < (chunk.byte_offset + chunk.nbytes));
        uint64_t vaddr = GPUModel::getMemAddr(
            chunk.rank, static_cast<uint64_t>(chunk.type), buf_byte_off);

        // Create a WFReq for this source chunk
        WFReqPtr wfreq_p =
            std::make_shared<WFReq>(wf_p, GpuInst::Load, vaddr, m_wfreq_nbytes);
        wfreq_lst.push_back(wfreq_p);
    }

    // Set instruction dependencies
    setInstDependencies(wfreq_lst);
    return wfreq_lst;
}

bool LoadOp::executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) {
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const GpuOp* op) {
            os << "LoadOp::executeFunctional -> " << op->to_string();
        },
        this);

    THROW_ERR_NR("[Msccl::LoadOp] LoadOp::executeFunctional is not yet "
                 "implemented: %s\n",
                 to_string());
    return true;
}

std::string LoadOp::to_string() const {
    std::stringstream ss;
    ss << "[LoadOp] unrolling=" << m_unrolling << ", niters=" << m_niters
       << ", src_chunks=[";
    for (size_t i = 0; i < m_src_chunks.size(); ++i) {
        const auto& chunk = m_src_chunks[i];
        ss << chunk.to_string();
        if (i != (m_src_chunks.size() - 1)) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

/**
 * StoreOp
 */
StoreOp::StoreOp(const std::vector<BufferChunk>& dst,
                 uint32_t unrolling,
                 uint32_t wfreq_nbytes,
                 uint64_t niters,
                 uint64_t iter_nbytes)
    : GpuOp("store", unrolling, niters),
      m_dst_chunks(dst) {
    // Sanity checks
    assert(wfreq_nbytes > 0);
    if (m_dst_chunks.empty()) {
        THROW_ERR_NR("StoreOp must have at least one destination chunk (%d)\n",
                     m_dst_chunks.size());
    }
    for (const auto& chunk : m_dst_chunks) {
        if (chunk.nbytes != m_dst_chunks[0].nbytes) {
            THROW_ERR_NR("StoreOp destination chunks must have the same size: "
                         "%d != %d\n",
                         chunk.nbytes, m_dst_chunks[0].nbytes);
        }
    }
    // Init data element info
    setOpSize(m_dst_chunks[0].nbytes, wfreq_nbytes, wfreq_nbytes, iter_nbytes);
}

std::vector<WFReqPtr> StoreOp::execute(Wavefront* wf_p, uint64_t eid) const {
    assert(wf_p);
    assert(eid < m_nelems);
    std::vector<WFReqPtr> wfreq_lst;
    for (const auto& chunk : m_dst_chunks) {
        // Compute the virtual address for the element index in the chunk
        uint64_t buf_byte_off = chunk.byte_offset + m_elem_nbytes * eid;
        assert(buf_byte_off < (chunk.byte_offset + chunk.nbytes));
        uint64_t vaddr = GPUModel::getMemAddr(
            chunk.rank, static_cast<uint64_t>(chunk.type), buf_byte_off);

        // Create a WFReq for this destination chunk
        WFReqPtr wfreq_p = std::make_shared<WFReq>(wf_p, GpuInst::Store, vaddr,
                                                   m_wfreq_nbytes);
        wfreq_lst.push_back(wfreq_p);
    }

    // Set instruction dependencies
    setInstDependencies(wfreq_lst);
    return wfreq_lst;
}

bool StoreOp::executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) {
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const GpuOp* op) {
            os << "StoreOp::executeFunctional -> " << op->to_string();
        },
        this);

    THROW_ERR_NR("[Msccl::StoreOp] StoreOp::executeFunctional is not yet "
                 "implemented: %s\n",
                 to_string());
    return true;
}

std::string StoreOp::to_string() const {
    std::stringstream ss;
    ss << "[StoreOp] unrolling=" << m_unrolling << ", niters=" << m_niters
       << ", dst_chunks=[";
    for (size_t i = 0; i < m_dst_chunks.size(); ++i) {
        const auto& chunk = m_dst_chunks[i];
        ss << chunk.to_string();
        if (i != (m_dst_chunks.size() - 1)) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

MemCpyOp::MemCpyOp(const std::string& op_name,
                   const std::vector<BufferChunk>& src,
                   const std::vector<BufferChunk>& dst,
                   uint32_t unrolling,
                   uint32_t wfreq_nbytes,
                   uint64_t niters,
                   uint64_t iter_nbytes)
    : GpuOp(op_name, unrolling, niters),
      m_src_chunks(src),
      m_dst_chunks(dst) {
    // Sanity checks
    assert(wfreq_nbytes > 0);
    if (m_src_chunks.empty()) {
        THROW_ERR_NR("MemCpyOp must have at least one source chunk (%d)\n",
                     m_src_chunks.size());
    }
    if (m_dst_chunks.empty()) {
        THROW_ERR_NR("MemCpyOp must have at least one destination chunk (%d)\n",
                     m_dst_chunks.size());
    }
    for (const auto& chunk : m_src_chunks) {
        if (chunk.nbytes != m_src_chunks[0].nbytes) {
            THROW_ERR_NR("MemCpyOp source chunks must have the same size: "
                         "%d != %d\n",
                         chunk.nbytes, m_src_chunks[0].nbytes);
        }
    }
    // For ruppkt, destination is both Input/Output and Scratch, so sizes will
    // be different, hence the exception
    for (const auto& chunk : m_dst_chunks) {
        if (chunk.nbytes != m_dst_chunks[0].nbytes && op_name != "ruppkt") {
            THROW_ERR_NR("MemCpyOp destination chunks must have the same size: "
                         "%d != %d\n",
                         chunk.nbytes, m_dst_chunks[0].nbytes);
        }
    }
    // source and destination chunks must have the same size when the
    // data_in_wfreq is the same as the wfreq_nbytes (Simple protocol)
    if (m_src_chunks[0].nbytes != m_dst_chunks[0].nbytes &&
        m_src_chunks[0].data_in_wfreq == wfreq_nbytes) {
        THROW_ERR_NR("MemCpyOp source and destination chunks must have the "
                     "same size for Simple protocol: %d != %d\n",
                     m_src_chunks[0].nbytes, m_dst_chunks[0].nbytes);
    }

    // Init data element info
    // m_src_chunks[0].nbytes is the chunk size in bytes (aligned to cache line)
    // wfreq_nbytes is the size of each wavefront request in bytes
    // m_src_chunks[0].data_in_wfreq is the data bytes to be accessed per wave
    // iter_nbytes is the data size of each iteration
    // setOpSize() is used to calculate number of data elements (m_nelems) and
    // number of iterations (m_niters). Element size is data_in_wfreq +
    // flag_in_wfreq (zero flag bytes for non-scratch / Simple protocol).
    uint32_t elem_nbytes =
        m_src_chunks[0].data_in_wfreq + m_src_chunks[0].flag_in_wfreq;
    DPRINTFNR("[MemCpyOp] Op %s setOpSize inputs:\n", op_name.c_str());
    for (size_t i = 0; i < m_src_chunks.size(); ++i) {
        const auto& c = m_src_chunks[i];
        DPRINTFNR("  src[%lu]: rank=%d type=%s off=0x%lx nbytes=%lu "
                  "data_in_wfreq=%u flag_in_wfreq=%u payload=%lu\n",
                  i, c.rank, bufferTypeToString(c.type), c.byte_offset,
                  c.nbytes, c.data_in_wfreq, c.flag_in_wfreq,
                  c.getPayloadBytes());
    }
    for (size_t i = 0; i < m_dst_chunks.size(); ++i) {
        const auto& c = m_dst_chunks[i];
        DPRINTFNR("  dst[%lu]: rank=%d type=%s off=0x%lx nbytes=%lu "
                  "data_in_wfreq=%u flag_in_wfreq=%u payload=%lu\n",
                  i, c.rank, bufferTypeToString(c.type), c.byte_offset,
                  c.nbytes, c.data_in_wfreq, c.flag_in_wfreq,
                  c.getPayloadBytes());
    }
    setOpSize(m_src_chunks[0].nbytes, wfreq_nbytes, elem_nbytes, iter_nbytes);
}

std::vector<WFReqPtr> MemCpyOp::execute(Wavefront* wf_p, uint64_t eid) const {
    assert(wf_p);
    assert(eid < m_nelems);
    std::vector<WFReqPtr> wfreq_lst;

    // Create exactly 1 WFReq per src chunk (may be misaligned).
    // Address alignment and splitting into cache-line-sized requests is
    // handled later by ComputeUnit's load-store queue at dispatch time.

    // Handling partial last element
    // If the last element, the request size may be less than the wfreq_nbytes
    // For example, for LL128 protocol, for ppkt,
    // if the chunk size is 1024 bytes and the wfreq_nbytes is 512 bytes and
    // data_in_wfreq is 480 bytes, then the request size will be 480 bytes for
    // local loads eid = 0 will have a request size of 480 bytes (data);
    // remaining bytes to be accessed are 1024 - 480 = 544 bytes eid = 1 will
    // have a request size of 480 bytes (data); remaining bytes to be accessed
    // are 544 - 480 = 64 bytes eid = 2 should have a request size of 64 bytes
    // (data); which is chunk size - (eid * request size) = 1024 - (2 * 480) =
    // 64 bytes Similarly for remote stores to Scratch, aligned Scratch size is
    // 1280 bytes eid = 0 will have a request size of 512 bytes (data+flags);
    // remaining bytes to be accessed are 1280 - 512 = 768 bytes eid = 1 will
    // have a request size of 512 bytes (data+flags); remaining bytes to be
    // accessed are 768 - 512 = 256 bytes eid = 2 should have a request size of
    // 256 bytes (data+flags); which is chunk size - (eid * request size) = 1280
    // - (2 * 512) = 256 bytes

    for (size_t chunk_idx = 0; chunk_idx < m_src_chunks.size(); ++chunk_idx) {
        const auto& chunk = m_src_chunks[chunk_idx];
        uint64_t request_nbytes = chunk.getPayloadBytes();
        uint64_t buf_byte_off = chunk.byte_offset + request_nbytes * eid;
        // If the last element, the request size may be less than the
        // wfreq_nbytes
        if (eid == (m_nelems - 1) && ((eid * request_nbytes) < chunk.nbytes)) {
            request_nbytes = std::min(request_nbytes,
                                      (chunk.nbytes - (eid * request_nbytes)));
        }
        DPRINTFNR("[MemCpyOp] Execute src chunk eid=%lu request_nbytes=%lu "
                  "buf_byte_off=0x%lx\n",
                  eid, request_nbytes, buf_byte_off);
        assert(buf_byte_off + request_nbytes <=
               (chunk.byte_offset + chunk.nbytes));

        uint64_t vaddr = GPUModel::getMemAddr(
            chunk.rank, static_cast<uint64_t>(chunk.type), buf_byte_off);

        WFReqPtr wfreq_p =
            std::make_shared<WFReq>(wf_p, GpuInst::Load, vaddr, request_nbytes);
        wfreq_lst.push_back(wfreq_p);
    }

    // WaitCnt to ensure loads complete before stores
    wfreq_lst.emplace_back(
        std::make_shared<WFReq>(wf_p, GpuInst::WaitCnt, 0, m_wfreq_nbytes));

    // Create exactly 1 WFReq per dst chunk (may be misaligned).
    for (const auto& chunk : m_dst_chunks) {
        uint64_t request_nbytes = chunk.getPayloadBytes();
        uint64_t buf_byte_off = chunk.byte_offset + request_nbytes * eid;
        // If the last element, the request size may be less than the
        // wfreq_nbytes
        if (eid == (m_nelems - 1) && ((eid * request_nbytes) < chunk.nbytes)) {
            request_nbytes = std::min(request_nbytes,
                                      (chunk.nbytes - (eid * request_nbytes)));
        }
        DPRINTFNR("[MemCpyOp] Execute dst chunk eid=%lu request_nbytes=%lu "
                  "buf_byte_off=0x%lx\n",
                  eid, request_nbytes, buf_byte_off);
        assert(buf_byte_off + request_nbytes <=
               (chunk.byte_offset + chunk.nbytes));

        uint64_t vaddr = GPUModel::getMemAddr(
            chunk.rank, static_cast<uint64_t>(chunk.type), buf_byte_off);

        WFReqPtr wfreq_p = std::make_shared<WFReq>(wf_p, GpuInst::Store, vaddr,
                                                   request_nbytes);
        wfreq_lst.push_back(wfreq_p);
    }

    // Set instruction dependencies
    setInstDependencies(wfreq_lst);
    return wfreq_lst;
}

bool MemCpyOp::executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) {
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const GpuOp* op) {
            os << "MemCpyOp::executeFunctional -> " << op->to_string();
        },
        this);

    // These operations can be either 1-to-1 (one input chunk goes to one output
    // chunk) or 1-to-many (one input chunk is broadcasted to many output
    // destinations).
    assert(m_src_chunks.size() == m_dst_chunks.size() ||
           m_src_chunks.size() == 1);
    bool is_one_to_one = m_src_chunks.size() == m_dst_chunks.size();
    for (int i = 0; i < m_dst_chunks.size(); i++) {
        BufferChunk& src_chunk = m_src_chunks[is_one_to_one ? i : 0];
        BufferChunk& dst_chunk = m_dst_chunks[i];
        FunctionalGPUModel::getGpuModelPtr(dst_chunk.rank)
            ->getMemoryPtr()
            ->set(FunctionalGPUModel::getGpuModelPtr(src_chunk.rank)
                      ->getMemoryPtr(),
                  src_chunk, dst_chunk);
    }

    // This operation never leads to a stall, the next executes immediately.
    return true;
}

std::string MemCpyOp::to_string() const {
    std::stringstream ss;
    ss << "[MemCpyOp] name=" << m_name << ", unrolling=" << m_unrolling
       << ", niters=" << m_niters << ", src_chunks=[";
    for (size_t i = 0; i < m_src_chunks.size(); ++i) {
        const auto& chunk = m_src_chunks[i];
        ss << chunk.to_string();
        if (i != (m_src_chunks.size() - 1)) {
            ss << ", ";
        }
    }
    ss << "], dst_chunks=[";
    for (size_t i = 0; i < m_dst_chunks.size(); ++i) {
        const auto& chunk = m_dst_chunks[i];
        ss << chunk.to_string();
        if (i != (m_dst_chunks.size() - 1)) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

MemCpyLLOp::MemCpyLLOp(const std::string& op_name,
                       const std::vector<BufferChunk>& src,
                       const std::vector<BufferChunk>& dst,
                       uint32_t unrolling,
                       uint32_t wfreq_nbytes,
                       uint64_t niters,
                       uint64_t iter_nbytes)
    : MemCpyOp(
          op_name, src, dst, unrolling, wfreq_nbytes, niters, iter_nbytes) {
    assert(m_src_chunks.size() > 0);

    // NOTE: This function makes the critical assumption that **any usage of the
    // Scratch buffer implies a Semaphore.** Thus, the below are true:
    //    * Sending a chunk from a source [input/output] to a destination
    //      [input/output] implies **no semaphore**.
    //    * Sending a chunk from a source [input/output] to a destination
    //      [scratch] implies the source sends a *signal* (SemReleaseOp) to the
    //      destination alongside the chunk.
    //    * Sending a chunk from a source [scratch] to a destination
    //      [input/output] implies the destination cannot *obtain* the chunk
    //      until a signal is seen associated with that chunk, implying a
    //      SemAcquireOp is attached to that chunk.
    //    * Sending a chunk from a source [scratch] to a destination [scratch]
    //      implies a SemAcquireOp *and* a SemReleaseOp together.
    //  The order of operations is
    //      [SemAcquireOps] -> [MemCpyOp] -> [SemReleaseOp]
    //  Thus, **all** SemAcquires must complete before the MemCpy or associated
    //  SemReleaseOps can begin.

    // Construct logical semaphores for functional simulation. These vectors
    // store the addresses of each semaphore.
    std::vector<uint64_t> sem_acquires;
    std::vector<uint64_t> sem_releases;

    for (const BufferChunk& src : m_src_chunks) {
        if (src.type == BufferType::Scratch) {
            sem_acquires.emplace_back(GPUModel::getMemAddr(
                src.rank, static_cast<uint64_t>(BufferType::Scratch),
                src.byte_offset));
        }
    }
    for (const BufferChunk& dst : m_dst_chunks) {
        if (dst.type == BufferType::Scratch) {
            sem_releases.emplace_back(GPUModel::getMemAddr(
                dst.rank, static_cast<uint64_t>(BufferType::Scratch),
                dst.byte_offset));
        }
    }

    // Create a single SemAcquireOp/SemReleaseOp representing the series of
    // Semaphore transactions, in order to prevent multiple semaphore acquires
    // in the executeFunctional phase.
    if (sem_acquires.size() > 0) {
        // BufferType is Scratch since the chunk itself is a semaphore.
        m_sem_acquire = std::make_unique<SemAcquireOp>(op_name, sem_acquires,
                                                       wfreq_nbytes, niters);
    } else {
        m_sem_acquire = nullptr;
    }

    if (sem_releases.size() > 0) {
        m_sem_release = std::make_unique<SemReleaseOp>(op_name, sem_releases,
                                                       wfreq_nbytes, niters);
    } else {
        m_sem_release = nullptr;
    }
}

bool MemCpyLLOp::executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) {
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const GpuOp* op) {
            os << "MemCpyLLOp::executeFunctional -> " << op->to_string();
        },
        this);

    // We can't copy data until we know the data was actually received.
    if (m_sem_acquire != nullptr) {
        if (!m_sem_acquire->executeFunctional(gpu, wg_p)) {
            return false;  // Come back later.
        }
    }

    // After this point, there is no chance of there being a stall. Copy each
    // source to each destination.
    MemCpyOp::executeFunctional(gpu, wg_p);

    // Tell the other rank(s) that we sent them data.
    if (m_sem_release != nullptr) {
        m_sem_release->executeFunctional(gpu, wg_p);
    }

    return true;
}

std::string MemCpyLLOp::to_string() const {
    std::stringstream ss;
    ss << "[MemCpyLLOp] name=" << m_name << ", unrolling=" << m_unrolling
       << ", niters=" << m_niters << ", src_chunks=[";
    for (size_t i = 0; i < m_src_chunks.size(); ++i) {
        const auto& chunk = m_src_chunks[i];
        ss << chunk.to_string();
        if (i != (m_src_chunks.size() - 1)) {
            ss << ", ";
        }
    }
    ss << "], dst_chunks=[";
    for (size_t i = 0; i < m_dst_chunks.size(); ++i) {
        const auto& chunk = m_dst_chunks[i];
        ss << chunk.to_string();
        if (i != (m_dst_chunks.size() - 1)) {
            ss << ", ";
        }
    }
    ss << "], m_sem_acquire=["
       << (m_sem_acquire == nullptr ? "nullptr" : m_sem_acquire->to_string())
       << "], m_sem_release=["
       << (m_sem_release == nullptr ? "nullptr" : m_sem_release->to_string())
       << "]";
    return ss.str();
}

/**
 * ReduceOp
 */
ReduceOp::ReduceOp(const std::string& op_name,
                   const std::vector<BufferChunk>& src,
                   const std::vector<BufferChunk>& dst,
                   uint32_t unrolling,
                   uint32_t wfreq_nbytes,
                   uint64_t niters,
                   uint64_t iter_nbytes)
    : GpuOp(op_name, unrolling, niters),
      m_src_chunks(src),
      m_dst_chunks(dst) {
    // Sanity checks
    assert(wfreq_nbytes > 0);
    if (m_src_chunks.empty()) {
        THROW_ERR_NR("ReduceOp must have at least one source chunk (%d)\n",
                     m_src_chunks.size());
    }
    if (m_dst_chunks.empty()) {
        THROW_ERR_NR("ReduceOp must have at least one destination chunk (%d)\n",
                     m_dst_chunks.size());
    }
    for (const auto& chunk : m_src_chunks) {
        if (chunk.nbytes != m_src_chunks[0].nbytes) {
            THROW_ERR_NR("ReduceOp source chunks must have the same size: "
                         "%d != %d\n",
                         chunk.nbytes, m_src_chunks[0].nbytes);
        }
    }
    for (const auto& chunk : m_dst_chunks) {
        if (chunk.nbytes != m_dst_chunks[0].nbytes) {
            THROW_ERR_NR("ReduceOp destination chunks must have the same size: "
                         "%d != %d\n",
                         chunk.nbytes, m_dst_chunks[0].nbytes);
        }
    }
    if (m_src_chunks[0].nbytes != m_dst_chunks[0].nbytes) {
        THROW_ERR_NR("ReduceOp source and destination chunks must have the "
                     "same size: %d != %d\n",
                     m_src_chunks[0].nbytes, m_dst_chunks[0].nbytes);
    }
    // Init data element info
    setOpSize(m_src_chunks[0].nbytes, wfreq_nbytes, wfreq_nbytes, iter_nbytes);
}

std::vector<WFReqPtr> ReduceOp::execute(Wavefront* wf_p, uint64_t eid) const {
    assert(wf_p);
    assert(eid < m_nelems);
    std::vector<WFReqPtr> wfreq_lst;

    // Create WFReqs for loading src chunks
    for (const auto& chunk : m_src_chunks) {
        // Compute the virtual address for the element index in the chunk
        uint64_t buf_byte_off = chunk.byte_offset + m_elem_nbytes * eid;
        assert(buf_byte_off < (chunk.byte_offset + chunk.nbytes));
        uint64_t vaddr = GPUModel::getMemAddr(
            chunk.rank, static_cast<uint64_t>(chunk.type), buf_byte_off);

        // Create a WFReq for this source chunk
        WFReqPtr wfreq_p =
            std::make_shared<WFReq>(wf_p, GpuInst::Load, vaddr, m_wfreq_nbytes);
        wfreq_lst.push_back(wfreq_p);
    }

    // Add a waitcnt to make sure ALL loads are done before reduce.
    // NOTE: we could potentially unlock one reduce as soon as its input from a
    // corresponding load is ready, instead of waiting for all loads to return.
    // TODO: further optimization in the future
    wfreq_lst.emplace_back(
        std::make_shared<WFReq>(wf_p, GpuInst::WaitCnt, 0, m_wfreq_nbytes));

    // Create WFReqs for the reduce op
    for (auto i = 0; i < m_src_chunks.size(); ++i) {
        WFReqPtr wfreq_p =
            std::make_shared<WFReq>(wf_p, GpuInst::Reduce, 0, m_wfreq_nbytes);
        wfreq_lst.push_back(wfreq_p);
    }

    // Add a waitcnt to make sure ALL reduces are done before stores
    wfreq_lst.emplace_back(
        std::make_shared<WFReq>(wf_p, GpuInst::WaitCnt, 0, m_wfreq_nbytes));
    // Finally, create WFReqs for storing dst chunks
    for (const auto& chunk : m_dst_chunks) {
        // Compute the virtual address for the element index in the chunk
        uint64_t buf_byte_off = chunk.byte_offset + m_elem_nbytes * eid;
        assert(buf_byte_off < (chunk.byte_offset + chunk.nbytes));
        uint64_t vaddr = GPUModel::getMemAddr(
            chunk.rank, static_cast<uint64_t>(chunk.type), buf_byte_off);

        // Create a WFReq for this destination chunk
        WFReqPtr wfreq_p = std::make_shared<WFReq>(wf_p, GpuInst::Store, vaddr,
                                                   m_wfreq_nbytes);
        wfreq_lst.push_back(wfreq_p);
    }

    // Set instruction dependencies
    setInstDependencies(wfreq_lst);
    return wfreq_lst;
}

bool ReduceOp::executeFunctional(FunctionalGPUModel* gpu, Workgroup* wg_p) {
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const GpuOp* op) {
            os << "ReduceOp::executeFunctional -> " << op->to_string();
        },
        this);

    // Is the destination chunk shared with the source?
    uint32_t shared_dst_idx;
    uint32_t shared_src_idx;
    bool shared = false;
    for (unsigned dst_idx = 0; dst_idx < m_dst_chunks.size(); dst_idx++) {
        for (unsigned src_idx = 0; src_idx < m_src_chunks.size(); src_idx++) {
            if (m_dst_chunks[dst_idx] == m_src_chunks[src_idx]) {
                shared = true;
                shared_dst_idx = dst_idx;
                shared_src_idx = src_idx;
                break;
            }
        }
        if (shared) {
            break;
        }
    }

    if (shared) {
        // If there's a shared destination and source, then reduce all sources
        // into that destination, and copy that destination into the other
        // destinations.
        BufferChunk* dst_chunk = &m_dst_chunks[shared_dst_idx];
        for (BufferChunk& src_chunk : m_src_chunks) {
            if (src_chunk != *dst_chunk) {
                FunctionalGPUModel::getGpuModelPtr(dst_chunk->rank)
                    ->getMemoryPtr()
                    ->reduce(FunctionalGPUModel::getGpuModelPtr(src_chunk.rank)
                                 ->getMemoryPtr(),
                             src_chunk, *dst_chunk);
            }
        }

        // Copy the shared destination to the other destinations.
        BufferChunk* src_chunk = dst_chunk;
        for (unsigned idx = 0; idx < m_dst_chunks.size(); idx++) {
            if (idx != shared_dst_idx) {
                dst_chunk = &m_dst_chunks[idx];
                FunctionalGPUModel::getGpuModelPtr(dst_chunk->rank)
                    ->getMemoryPtr()
                    ->set(FunctionalGPUModel::getGpuModelPtr(src_chunk->rank)
                              ->getMemoryPtr(),
                          *src_chunk, *dst_chunk);
            }
        }
    } else {
        // If there is no shared destination and source, then copy the first
        // source into the first destination.
        BufferChunk* dst_chunk = &m_dst_chunks[0];
        BufferChunk* src_chunk = &m_src_chunks[0];
        FunctionalGPUModel::getGpuModelPtr(dst_chunk->rank)
            ->getMemoryPtr()
            ->set(FunctionalGPUModel::getGpuModelPtr(src_chunk->rank)
                      ->getMemoryPtr(),
                  *src_chunk, *dst_chunk);

        // Reduce the remaining sources into that same destination.
        for (unsigned idx = 1; idx < m_src_chunks.size(); idx++) {
            src_chunk = &m_src_chunks[idx];
            FunctionalGPUModel::getGpuModelPtr(dst_chunk->rank)
                ->getMemoryPtr()
                ->reduce(FunctionalGPUModel::getGpuModelPtr(src_chunk->rank)
                             ->getMemoryPtr(),
                         *src_chunk, *dst_chunk);
        }

        // Copy the first destination to the other destinations.
        src_chunk = dst_chunk;
        for (unsigned idx = 1; idx < m_dst_chunks.size(); idx++) {
            dst_chunk = &m_dst_chunks[idx];
            FunctionalGPUModel::getGpuModelPtr(dst_chunk->rank)
                ->getMemoryPtr()
                ->set(FunctionalGPUModel::getGpuModelPtr(src_chunk->rank)
                          ->getMemoryPtr(),
                      *src_chunk, *dst_chunk);
        }
    }

    // Execute next instruction immediately.
    return true;
}

std::string ReduceOp::to_string() const {
    std::stringstream ss;
    ss << "[ReduceOp] name=" << m_name << ", unrolling=" << m_unrolling
       << ", niters=" << m_niters << ", src_chunks=[";
    for (size_t i = 0; i < m_src_chunks.size(); ++i) {
        const auto& chunk = m_src_chunks[i];
        ss << chunk.to_string();
        if (i != (m_src_chunks.size() - 1)) {
            ss << ", ";
        }
    }
    ss << "], dst_chunks=[";
    for (size_t i = 0; i < m_dst_chunks.size(); ++i) {
        const auto& chunk = m_dst_chunks[i];
        ss << chunk.to_string();
        if (i != (m_dst_chunks.size() - 1)) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}
