// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <memory>

#include <astra-sim/common/AstraNetworkAPI.hh>
#include <astra-sim/system/gpu/GpuOp.hh>

namespace AstraSim {

class Wavefront;

//------------------------------------------------------------------------------
// WFReq
//------------------------------------------------------------------------------
// Modeling a single wavefront memory instruction (load/store/etc.).
// reqSizeNBytes is the total logical access size; the CU may split it into
// multiple cache-line-sized network sub-requests via the LSQ.
// Each WFReq has its globally unique ID.

struct WFReq {
    enum class Status {
        Dispatched,  // Dispatched from an active WF
        Issued,      // Issued to network model
        Blocked,     // Blocked by network model
        Completed,   // Got an ACK from network -> finished!
        Invalid
    };

    // Constructor & Destructor
    WFReq(Wavefront* _wf_p,
          GpuInst _inst,
          uint64_t _vaddr,
          uint32_t _reqSizeNBytes);
    ~WFReq() = default;

    // For printing & logging
    std::string to_string() const;
    friend std::ostream& operator<<(std::ostream& os, const WFReq& wfreq) {
        os << wfreq.to_string();
        return os;
    }

    // Return a network sim request. Generates a
    // unique sub-request ID for the WFReq
    sim_request genSimRequest(uint64_t addr, uint64_t req_size_nbytes) const;

    // Return network command type of this request
    AstraNetworkAPI::CommandType getAstraNetCmdType(uint32_t loc_gpu_id) const;

    // Public members
    const uint64_t wfreq_id;  // AstraSim unique ID of this WF request
    const GpuInst inst;       // GPU instruction type
    const uint64_t vaddr;     // Target virtual address
    Wavefront* wf_p;          // Pointer to the issuing wavefront
    Status status;            // current state of this request
    uint32_t retry_cnt;       // Retrying count (for deadlock check)
    Tick issue_tick;          // Tick when this request is issued by CU
    uint32_t waitcnt;         // Only for waitcnt inst
    uint32_t
        reqSizeNBytes;  // Total logical access size for this WF inst (bytes)
    // Pointer to a waitcnt request waiting for this request to complete
    std::shared_ptr<WFReq> waitcnt_wfreq_p;
};

typedef std::shared_ptr<WFReq> WFReqPtr;

}  // namespace AstraSim
