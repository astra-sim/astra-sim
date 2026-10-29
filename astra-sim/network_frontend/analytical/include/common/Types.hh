// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <cstdint>
#include <ostream>
#include <queue>
#include <unordered_map>
#include <vector>

// for sim_request
using namespace AstraSim;

namespace AstraSimAnalytical {

enum class AllChunkStates {
    // in order of priority
    // always want to drain so those closer to exiting are prioritized
    Invalid = 0,  // 0 valued for assertions
    Completed,
    Completed_Barrier,
    DstBufToSrcCU_AckCtrl,
    DstBufToSrcCU_Data,
    // DstBufToDstCU_Data, // unused
    // WaitingForRecv, // unused
    SrcToDstGPU_Data,  // plain rank-to-rank Send, no buffer addressing
    SrcCUToDstBuf_Data,
    SrcBufToSrcCU_AckCtrl,
    SrcBufToSrcCU_Data,
    SrcCUToDstBuf_ReqCmd,
    SrcCUToSrcBuf_ReqCmd,
    SrcCUToSrcBuf_Data,
    Unstarted,
};

// How resources are labeled. Open to change later
typedef int ResourceID;

// How requests are labeled
// match sim_request::astraReqId type
typedef uint64_t RequestID;

typedef uint32_t Sys_ID;
typedef uint64_t Buf_Offset;
typedef uint32_t CU_ID;

typedef std::unordered_map<ResourceID, std::queue<RequestID>>
    ResourceToRequestsMap;
typedef std::unordered_map<RequestID, std::vector<ResourceID>>
    RequestToResourcesMap;

// recreation of sim_request
// minimal amount of entries as this will be replicated many times
struct analytical_network_request {
    // default
    analytical_network_request() {}

    analytical_network_request(const sim_request* sim_req)
        : srcRank(-1),
          dstRank(sim_req->node),
          reqSizeNBytes(sim_req->reqSizeNBytes),
          bufByteOff(sim_req->bufByteOff),
          reqCuId(sim_req->reqCuId) {}

    // all same name as in sim_request
    Sys_ID srcRank;
    Sys_ID dstRank;
    uint64_t reqSizeNBytes;
    Buf_Offset bufByteOff;
    CU_ID reqCuId;
};

}  // namespace AstraSimAnalytical
