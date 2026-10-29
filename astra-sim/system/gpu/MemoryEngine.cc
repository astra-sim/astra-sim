// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/gpu/MemoryEngine.hh>

#include <algorithm>

#include <astra-sim/system/gpu/WavefrontReq.hh>

using namespace AstraSim;

//------------------------------------------------------------------------------
// LoadStoreQueue
//------------------------------------------------------------------------------

LoadStoreQueue::LoadStoreQueue(uint32_t depth, uint32_t req_size)
    : m_depth(depth),
      m_req_size(req_size) {}

bool LoadStoreQueue::isFull() const {
    return m_entries.size() >= m_depth;
}

uint32_t LoadStoreQueue::getReqSize() const {
    return m_req_size;
}

uint32_t LoadStoreQueue::size() const {
    return static_cast<uint32_t>(m_entries.size());
}

std::vector<uint64_t> LoadStoreQueue::computeAlignedAddresses(
    WFReqPtr wfreq_p) const {
    uint64_t vaddr = wfreq_p->vaddr;
    uint64_t req_size = m_req_size;

    // BUF_BYTE_OFF occupies the lowest bits of vaddr, so aligning vaddr
    // to req_size boundaries is equivalent to aligning the byte offset.
    uint64_t start_aligned = vaddr & ~(req_size - 1);
    uint64_t end = vaddr + wfreq_p->reqSizeNBytes;

    std::vector<uint64_t> addrs;
    for (uint64_t a = start_aligned; a < end; a += req_size) {
        addrs.push_back(a);
    }
    return addrs;
}

uint32_t LoadStoreQueue::getNumLSQEntries(WFReqPtr wfreq_p) const {
    return static_cast<uint32_t>(computeAlignedAddresses(wfreq_p).size());
}

void LoadStoreQueue::insert(WFReqPtr wfreq_p) {
    insert(wfreq_p->wfreq_id, wfreq_p, computeAlignedAddresses(wfreq_p));
}

void LoadStoreQueue::insert(uint64_t wfreq_id,
                            WFReqPtr wfreq_p,
                            const std::vector<uint64_t>& aligned_addrs) {
    assert(!isFull());
    LSQEntry entry;
    entry.wfreq_p = wfreq_p;
    entry.mem_reqs.reserve(aligned_addrs.size());
    for (uint64_t addr : aligned_addrs) {
        entry.mem_reqs.push_back({LSQEntry::MemReqStatus::Pending, addr, 0});
    }
    entry.num_pending = static_cast<uint32_t>(aligned_addrs.size());
    entry.num_in_flight = 0;
    entry.num_completed = 0;
    m_entries[wfreq_id] = std::move(entry);
}

LSQEntry* LoadStoreQueue::getEntry(uint64_t wfreq_id) {
    auto it = m_entries.find(wfreq_id);
    return (it != m_entries.end()) ? &it->second : nullptr;
}

void LoadStoreQueue::remove(uint64_t wfreq_id) {
    auto it = m_entries.find(wfreq_id);
    assert(it != m_entries.end());
    m_entries.erase(it);
}

//------------------------------------------------------------------------------
// MemoryEngine
//------------------------------------------------------------------------------

// Builds the per-CU load-store queue and records whether the MSHR is enabled.
MemoryEngine::MemoryEngine(uint32_t lsq_depth,
                           uint32_t lsq_req_size,
                           bool use_mshr)
    : m_lsq(std::make_unique<LoadStoreQueue>(lsq_depth, lsq_req_size)),
      m_use_mshr(use_mshr) {}

LoadStoreQueue* MemoryEngine::lsq() {
    return m_lsq.get();
}

const LoadStoreQueue* MemoryEngine::lsq() const {
    return m_lsq.get();
}

bool MemoryEngine::useMshr() const {
    return m_use_mshr;
}

bool MemoryEngine::usesLSQ(GpuInst inst) const {
    return inst != GpuInst::Reduce && inst != GpuInst::WaitCnt;
}

uint32_t MemoryEngine::getNumLSQEntries(WFReqPtr wfreq_p) const {
    if (!usesLSQ(wfreq_p->inst)) {
        return 0;
    }
    return m_lsq->getNumLSQEntries(wfreq_p);
}

void MemoryEngine::insertWFReqToLSQ(WFReqPtr wfreq_p) {
    assert(usesLSQ(wfreq_p->inst));
    m_lsq->insert(wfreq_p);
}

void MemoryEngine::removeWFReqFromLSQ(WFReqPtr wfreq_p) {
    assert(usesLSQ(wfreq_p->inst));
    m_lsq->remove(wfreq_p->wfreq_id);
}

bool MemoryEngine::insertMSHR(uint64_t vaddr, uint64_t wfreq_id) {
    auto it = m_mshr.find(vaddr);
    if (it != m_mshr.end()) {
        auto& waiters = it->second;
        // Primary re-issue (e.g. sem acquire poll after memory response): allow
        // another sim_cmd without coalescing or duplicating the waiter list.
        if (!waiters.empty() && waiters.front() == wfreq_id) {
            return true;
        }
        if (std::find(waiters.begin(), waiters.end(), wfreq_id) ==
            waiters.end()) {
            waiters.push_back(wfreq_id);
        }
        return false;  // Dependent: coalesces with existing in-flight load
    }
    m_mshr[vaddr] = {wfreq_id};
    return true;  // Primary: should issue to network
}

void MemoryEngine::completeMSHRAddr(uint64_t vaddr,
                                    const FinalizeCallback& finalize_cb) {
    auto it = m_mshr.find(vaddr);
    if (it == m_mshr.end()) {
        return;
    }

    for (uint64_t wid : it->second) {
        auto* entry = m_lsq->getEntry(wid);
        if (!entry) {
            continue;
        }

        int idx = entry->findAddrIndex(vaddr);
        if (idx < 0) {
            continue;
        }

        entry->markComplete(idx);
        if (entry->isAllComplete() && finalize_cb) {
            finalize_cb(entry->wfreq_p);
        }
    }
    m_mshr.erase(it);
}
