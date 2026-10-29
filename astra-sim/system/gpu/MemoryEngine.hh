// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include <astra-sim/system/gpu/GpuOp.hh>

namespace AstraSim {

class WFReq;
typedef std::shared_ptr<WFReq> WFReqPtr;

/**
 * LSQEntry: tracks cache-line-aligned memory addresses generated from a
 * single wfreq.  A wfreq of size W bytes at cache-line granularity G produces
 * ceil((misalignment + W) / G) addresses, all stored in one entry.
 */
struct LSQEntry {
    enum class MemReqStatus {
        Pending,    // Not yet issued (or needs re-issue after block/sem retry)
        InFlight,   // Awaiting response (primary on network, or MSHR coalesced)
        Completed,  // Response received
    };

    struct MemReq {
        MemReqStatus status = MemReqStatus::Pending;
        uint64_t vaddr = 0;
        uint64_t astra_req_id = 0;
    };

    WFReqPtr wfreq_p;
    std::vector<MemReq> mem_reqs;
    uint32_t num_pending = 0;
    uint32_t num_in_flight = 0;
    uint32_t num_completed = 0;

    bool isAllComplete() const {
        return num_completed == mem_reqs.size();
    }

    bool hasPending() const {
        return num_pending != 0;
    }

    bool hasInFlight() const {
        return num_in_flight != 0;
    }

    void setMemReqStatus(size_t idx, MemReqStatus new_status) {
        assert(new_status != MemReqStatus::Completed);
        MemReq& req = mem_reqs[idx];
        const MemReqStatus old = req.status;
        if (old == new_status) {
            return;
        }
        assert(old != MemReqStatus::Completed);

        if (old == MemReqStatus::Pending) {
            assert(num_pending > 0);
            num_pending--;
        } else {
            assert(num_in_flight > 0);
            num_in_flight--;
        }

        if (new_status == MemReqStatus::Pending) {
            num_pending++;
        } else {
            num_in_flight++;
        }
        req.status = new_status;
    }

    void markComplete(size_t idx) {
        MemReq& req = mem_reqs[idx];
        if (req.status == MemReqStatus::Completed) {
            return;
        }
        if (req.status == MemReqStatus::Pending) {
            assert(num_pending > 0);
            num_pending--;
        } else {
            assert(num_in_flight > 0);
            num_in_flight--;
        }
        req.status = MemReqStatus::Completed;
        num_completed++;
    }

    void assertStatusCounts() const {
#ifndef NDEBUG
        uint32_t pending = 0;
        uint32_t in_flight = 0;
        uint32_t completed = 0;
        for (const MemReq& req : mem_reqs) {
            switch (req.status) {
            case MemReqStatus::Pending:
                pending++;
                break;
            case MemReqStatus::InFlight:
                in_flight++;
                break;
            case MemReqStatus::Completed:
                completed++;
                break;
            }
        }
        assert(pending == num_pending);
        assert(in_flight == num_in_flight);
        assert(completed == num_completed);
        assert(pending + in_flight + completed == mem_reqs.size());
#endif
    }

    int findAddrIndex(uint64_t vaddr) const {
        for (size_t i = 0; i < mem_reqs.size(); i++) {
            if (mem_reqs[i].vaddr == vaddr) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    static const char* getMemReqStatusName(MemReqStatus st) {
        switch (st) {
        case MemReqStatus::Pending:
            return "Pending";
        case MemReqStatus::InFlight:
            return "InFlight";
        case MemReqStatus::Completed:
            return "Completed";
        default:
            return "Unknown";
        }
    }
};

/**
 * LoadStoreQueue: per-ComputeUnit structure that tracks memory addresses
 * generated from load/store wfreqs at cache-line granularity.
 * Configurable depth and request size (cache-line granularity).
 */
class LoadStoreQueue {
  public:
    LoadStoreQueue(uint32_t depth, uint32_t req_size);

    // True when the queue holds `depth` entries.
    bool isFull() const;

    // Cache-line granularity (bytes) of the tracked addresses.
    uint32_t getReqSize() const;

    // Number of entries currently in the queue.
    uint32_t size() const;

    // CU outstanding slots consumed by this wfreq.
    uint32_t getNumLSQEntries(WFReqPtr wfreq_p) const;

    // Insert a wfreq into the queue (aligned addrs computed internally).
    void insert(WFReqPtr wfreq_p);

    // Entry for the given wfreq id, or nullptr if absent.
    LSQEntry* getEntry(uint64_t wfreq_id);

    // Remove a completed entry from the queue.
    void remove(uint64_t wfreq_id);

  private:
    std::vector<uint64_t> computeAlignedAddresses(WFReqPtr wfreq_p) const;

    void insert(uint64_t wfreq_id,
                WFReqPtr wfreq_p,
                const std::vector<uint64_t>& aligned_addrs);

    uint32_t m_depth;
    uint32_t m_req_size;
    std::unordered_map<uint64_t, LSQEntry> m_entries;
};

/**
 * MemoryEngine: per-ComputeUnit facade for memory-movement structures
 * (load-store queue, MSHR, and future extensions).
 */
class MemoryEngine {
  public:
    using FinalizeCallback = std::function<void(WFReqPtr)>;

    MemoryEngine(uint32_t lsq_depth, uint32_t lsq_req_size, bool use_mshr);

    LoadStoreQueue* lsq();
    const LoadStoreQueue* lsq() const;

    bool useMshr() const;

    // True for all mem ops that use the LSQ (everything except Reduce/WaitCnt).
    bool usesLSQ(GpuInst inst) const;

    // CU outstanding LSQ slots (0 when usesLSQ is false).
    uint32_t getNumLSQEntries(WFReqPtr wfreq_p) const;

    // Insert a wfreq into the LSQ.
    void insertWFReqToLSQ(WFReqPtr wfreq_p);

    // Remove a wfreq from the LSQ.
    void removeWFReqFromLSQ(WFReqPtr wfreq_p);

    // Insert an MSHR entry for the given aligned vaddr.
    // Returns true if this wfreq should issue to network (new or existing
    // primary); false if it coalesces as a dependent. The first waiter is the
    // primary; a later insertMSHR from that same wfreq_id re-issues (sem
    // retry).
    bool insertMSHR(uint64_t vaddr, uint64_t wfreq_id);

    // Handle MSHR completion for a vaddr: marks the address complete in
    // all dependent LSQ entries and invokes finalize_cb for fully-done wfreqs.
    void completeMSHRAddr(uint64_t vaddr, const FinalizeCallback& finalize_cb);

  private:
    std::unique_ptr<LoadStoreQueue> m_lsq;
    const bool m_use_mshr;

    // Maps aligned vaddr -> list of wfreq IDs that need this address.
    // The first ID is the primary (issued to network); the rest coalesce.
    std::unordered_map<uint64_t, std::vector<uint64_t>> m_mshr;
};

}  // namespace AstraSim
