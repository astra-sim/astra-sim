// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cstdint>

namespace AstraSim {

typedef uint64_t Tick;

constexpr uint64_t CLOCK_PERIOD = 1;  // 1ps
constexpr uint64_t FREQ = 1000 * 1000 * 1000;

enum time_type_e { SE = 0, MS, US, NS, PS, FS };

enum req_type_e { UINT8 = 0, BFLOAT16, FP32 };

struct timespec_t {
    time_type_e time_res;
    Tick time_val;
    timespec_t() {}
    timespec_t(time_type_e t, Tick v) : time_res(t), time_val(v) {}
};

struct sim_request {
    uint64_t astraReqId = 0;     // unique request ID within AstraSim
    uint32_t reqCuId = 0;        // ID of requesting CU in a requesting GPU
    uint64_t reqSizeNBytes = 0;  // request size in bytes
    uint32_t node = 0;           // target node ID
    uint64_t vaddr = 0;          // target virtual address
    /** miscs (for backward compatibility) */
    int32_t buf = 0;          // target buffer type
    uint64_t bufByteOff = 0;  // target buffer byte offset
    uint32_t srcRank = 0;
    uint32_t dstRank = 0;
    uint32_t tag = 0;
    req_type_e reqType = req_type_e::UINT8;
    uint32_t vnet = 0;
    uint32_t layerNum = 0;
};

class MetaData {
  public:
    timespec_t timestamp;
};

enum class ComType {
    None = 0,
    Reduce_Scatter,
    All_Gather,
    All_Reduce,
    All_to_All,
    All_Reduce_All_to_All
};

enum class CollectiveOptimization { Baseline = 0, LocalBWAware };

enum class CollectiveImplType {
    Ring = 0,
    OneRing,
    Direct,
    OneDirect,
    AllToAll,
    DoubleBinaryTreeLocalAllToAll,
    LocalRingNodeA2AGlobalDBT,
    HierarchicalRing,
    DoubleBinaryTree,
    HalvingDoubling,
    OneHalvingDoubling,
};

enum class CollectiveBarrier { Blocking = 0, Non_Blocking };

enum class SchedulingPolicy { LIFO = 0, FIFO, EXPLICIT, None };

enum class IntraDimensionScheduling {
    FIFO = 0,
    RG,
    SmallestFirst,
    LessRemainingPhaseFirst
};

enum class InterDimensionScheduling {
    Ascending = 0,
    OnlineGreedy,
    RoundRobin,
    OfflineGreedy,
    OfflineGreedyFlex
};

enum class InjectionPolicy {
    Infinite = 0,
    Aggressive,
    SemiAggressive,
    ExtraAggressive,
    Normal,
    LDSTBased
};

enum class PacketRouting { Hardware = 0, Software };

enum class BusType { Both = 0, Shared, Mem };

enum class StreamState {
    Created = 0,
    Transferring,
    Ready,
    Executing,
    Zombie,
    Dead
};

enum class EventType {
    CallEvents = 0,
    General,
    RendezvousSend,
    RendezvousRecv,
    PacketReceived,
    PacketSent,
    PacketCmdAcked,
    PacketTxAck,
    Rec_Finished,
    Send_Finished,
    Processing_Finished,
    NPU_to_MA,
    MA_to_NPU,
    Consider_Process,
    Consider_Retire,
    Consider_Send_Back,
    StreamInit,
    CommProcessingFinished,
    CollectiveCommunicationFinished,
    CompFinished,
    IssueWFReq,
    DispatchInflightWFReq,
    LocalReqDispatch,
    MemLoadFinished,
    MemStoreFinished,
    CompletionReceived,
    Workload_NodeFinished,      // Event to inform workload that a node finished
    GPUModel_IssueNodes,        // Event to issue a node to a WG
    GPUModel_SemaphoreTimeout,  // Event to check for semaphore timeout
    GPUModel_ReExecOps,         // Event to execute each pending operation again
    CU_DispatchWFReqs,          // Event to dispatch ready WGs to free WFs
    CU_IssueWFReq,              // Event to issue a single WF req to network
    CU_WakeupWFReq,             // Event to either unblock or complete a WF req
    CU_UnstallWorkgroup,        // Event to unstall a workgroup
    CE_ExecWFReq,               // Event to execute a compute engine WF req
    UndefinedEvent              // Represents an event which shouldn't be called
};

class CloneInterface {
  public:
    virtual CloneInterface* clone() const = 0;
    virtual ~CloneInterface() = default;
};

class CollectiveImpl : public CloneInterface {
  public:
    CollectiveImpl(CollectiveImplType type) {
        this->type = type;
    };
    virtual CloneInterface* clone() const {
        return new CollectiveImpl(*this);
    }

    CollectiveImplType type;
};

class DirectCollectiveImpl : public CollectiveImpl {
  public:
    CloneInterface* clone() const {
        return new DirectCollectiveImpl(*this);
    };
    DirectCollectiveImpl(CollectiveImplType type, int direct_collective_window)
        : CollectiveImpl(type) {
        this->direct_collective_window = direct_collective_window;
    }

    int direct_collective_window;
};

}  // namespace AstraSim
