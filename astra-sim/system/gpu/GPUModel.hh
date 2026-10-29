// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cassert>
#include <cstdio>
#include <deque>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <astra-sim/common/Common.hh>
#include <astra-sim/common/SimObject.hh>
#include <astra-sim/common/stats/Statistics.hh>
#include <astra-sim/system/common/BasicEventHandlerData.hh>
#include <astra-sim/system/common/CallData.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/gpu/Memory.hh>
#include <astra-sim/system/gpu/Workgroup.hh>
#include <astra-sim/system/gpu/msccl/Type.hh>
#include <feeder/wrapper_node.h>

/**
 * Interface with System layer to manage GPU requests
 */

namespace AstraSim {
class GpuSys;
class GPUModel;
class ComputeUnit;
class Workload;
class WFReq;
namespace Msccl {
class GpuParser;
};

enum SemaphorePeekStatus {
    // The semaphore was registered, but an acquire of this semaphore would
    // fail.
    RegisterNoAcquire,

    // The semaphore was registered, and an acquire of this semaphore would
    // succeed.
    RegisterYesAcquire,

    // The semaphore was not registered.
    NoRegister
};

/**
 * Data needed for callback functions which execute a workgroup on a particular
 * GPU (e.g., when semaphores are acquired).
 */
class WorkgroupExecHandlerData : public BasicEventHandlerData {
  public:
    WorkgroupExecHandlerData();
    WorkgroupExecHandlerData(int _sys_id, Workgroup* _wg_p)
        : BasicEventHandlerData(_sys_id, EventType::GPUModel_ReExecOps),
          wg_p(_wg_p) {}

    Workgroup* wg_p;
};

class GPUModel : public SimObject, public Callable {
  protected:
    // Representing a single kernel launch with a collection of WGs
    struct Kernel {
        int node_id;  // Chakra node ID
        // Maps a workgroup ID to the Workgroup it represents.
        std::unordered_map<uint32_t, Workgroup*> wg_lst;
        uint32_t num_outstanding_wgs;
        Kernel(int _node_id, uint32_t num_wgs);
        ~Kernel() = default;
    };

    // Representing a workgroup barrier
    struct WorkgroupBarrier {
        // Set of WGs that have reached this barrier
        std::unordered_set<Workgroup*> waiting_wg_set;
        // Total number of WGs expected to reach this barrier
        uint32_t num_wgs;
        WorkgroupBarrier(uint32_t _num_wgs);
        ~WorkgroupBarrier() = default;
    };

  public:
    struct CallbackFunc {
        void (*func)(void*);
        void* args;
        CallbackFunc(void (*_func)(void*), void* _args)
            : func(_func),
              args(_args) {}
    };
    struct CallbackFuncWithTime {  // Changed from tuple to be more strict
        void (*func)(void*);
        void* args;
        Tick register_tick;        // When this callback was registered
        bool was_callback_called;  // True if callback was already invoked; we
                                   // will not double-invoke the same callback
        CallbackFuncWithTime(void (*_func)(void*),
                             void* _args,
                             Tick _register_tick,
                             bool _was_callback_called)
            : func(_func),
              args(_args),
              register_tick(_register_tick),
              was_callback_called(_was_callback_called) {}
    };

    GPUModel(GpuSys* sys,
             Workload* wl,
             int gpu_id,
             enum Memory::MemoryMode memory_mode,
             timespec_t acquire_delay);
    ~GPUModel();

    // TODO Make cache-line offset bits a parameter
    static constexpr uint64_t CLINE_OFFSET_NBITS = 8;
    static constexpr uint64_t CLINE_NBYTES = 1ULL << CLINE_OFFSET_NBITS;
    static constexpr uint64_t CLINE_ADDR_MASK = CLINE_NBYTES - 1;

    /**
     * Layout for non-synchronization addresses
     *
     * +--------+----------+------------------------------------------+
     * | GPU ID | buf type | buf byte offset                          |
     * +--------+----------+------------------------------------------+
     * | 17b    | 3b       | 37b                                      |
     * +--------+----------+------------------------------------------+
     *
     * Layout for semaphore addresses
     *
     * +--------+----------+------------------+--------------+--------+
     * | GPU ID | buf type | Connected GPU ID | Semaphore ID | zeros  |
     * +--------+----------+------------------+--------------+--------+
     * | 17b    | 3b       | 17b              | 12b          | 8b     |
     * +--------+----------+------------------+--------------+--------+
     *
     */
    static constexpr uint32_t GPU_ID_POS = 40;
    static constexpr uint32_t GPU_ID_LEN = 17;
    static constexpr uint32_t BUF_TYPE_POS = 37;
    static constexpr uint32_t BUF_TYPE_LEN = 3;
    static constexpr uint32_t BUF_BYTE_OFF_POS = 0;
    static constexpr uint32_t BUF_BYTE_OFF_LEN = 37;
    static constexpr uint32_t SEM_CONNECTED_GPU_ID_POS = 20;
    static constexpr uint32_t SEM_CONNECTED_GPU_ID_LEN = 17;
    static constexpr uint32_t SEM_ID_POS = 8;
    static constexpr uint32_t SEM_ID_LEN = 12;

    // Representing a semaphore
    // We maintain a pair of expected value and actual value for each
    // semaphore. A acquire can proceed when the actual value is greater than
    // or equal to the expected value. The expected value is incremented by
    // acquire, and the actual value is incremented by release. This way, we
    // can support out-of-order releases and multiple acquires/releases on the
    // same semaphore.
    // Potentially, there can be multiple pending acquires for the same
    // semaphore. However, in reality, we expect only one wavefront is
    // acquiring a given semaphore at a time.
    struct Semaphore {
        int expect_value = 0;
        int actual_value = 0;
        std::deque<CallbackFuncWithTime> pending_acquires;
        Semaphore() = default;
        ~Semaphore() = default;
    };

    // Turn a tuple of (gpu_id, buffer type, and buffer byte offset) into a
    // virtual non-synchronization memory address
    static uint64_t getMemAddr(uint64_t gpu,
                               uint64_t buf,
                               uint64_t buf_byte_off);

    // Return memory address for semaphores (with the option to choose the
    // storage location of the semaphore)
    static uint64_t getSemAddr(
        uint64_t gpu,
        uint64_t connected_gpu,
        uint64_t sem_id,
        Msccl::BufferType buf_type = Msccl::BufferType::Semaphore);

    // Return true if the given address is a semaphore address
    bool isSemAddr(uint64_t vaddr) const;

    // Return a cache-line-aligned address of a given address
    static uint64_t getCacheLineAddr(uint64_t vaddr);

    // Return the GPU that owns a given address
    static uint64_t getGpuIdFromAddr(uint64_t vaddr);

    // Return the dst buffer type from a given address
    static uint64_t getBufTypeFromAddr(uint64_t vaddr);

    // Return the dst buffer byte offset from a given address
    static uint64_t getBufByteOffFromAddr(uint64_t vaddr);

    // Insert a new wrapper node into the ready list
    void insertWrapperNode(WrapperNode& node);

    // Return true if there is ready node to issue
    bool hasReadyNodes() const;

    // Issue the next ready wrapper nodes in the list
    void issueNextReadyNodes();

    // Returns true if we can launch all of the workgroups, or false if there is
    // any structural constraint preventing us from launching all workgroups.
    virtual bool canLaunch(uint32_t node_id, uint32_t num_wgs) = 0;

    // Launches the workgroup ID.
    virtual void launch(const Msccl::GpuParser& msccl_parser,
                        Kernel& kernel,
                        uint32_t wg_id) = 0;

    // Called when a workgroup is done.
    void completeWorkgroup(Workgroup* wg);

    /**
     * Semaphore-related functions
     */

    // Register an acquire op for a semaphore. This function creates a new
    // entry (if not existing) and increments the expected value for the
    // semaphore.
    void registerAcquireSemaphore(uint64_t sem_addr,
                                  const CallbackFunc& callback_func);

    // Returns true if we can acquire this semaphore immediately.
    SemaphorePeekStatus peekSemaphore(uint64_t sem_addr);

    // Acquire a semaphore: Return true if the acquire can proceed immediately.
    // Otherwise, return false and the acquire will be completed when the
    // expected value is reached.
    bool acquireSemaphore(uint64_t sem_addr);

    // Release a semaphore
    // Return true if the release was successful. Otherwise, return false
    // (e.g., the given address is not associated with any pending semaphore).
    // If immediate is true, then the associated callback function is
    // immediately executed; if it's false, then the callback function is
    // executed after the acquire delay time passes.
    bool releaseSemaphore(uint64_t sem_addr);

    // name() for SimObject
    const std::string& name() const override;

    // curTick() for SimObject
    Tick curTick() const override;

    // call() for Callable
    void call(EventType type, CallData* data) override;

    // Returns true if the memory is correct, or false if it's incorrect. Prints
    // all incorrect aspects to stderr.
    bool verify() const;

    Msccl::ProtocolType getProtocolType() const;

    // Get GPU ID
    int getId() const;

    // Get System pointer
    GpuSys* getSysPtr() const;

    // Get Workload pointer
    Workload* getWorkloadPtr() const;

    // Get Memory pointer
    Memory* getMemoryPtr() const;

  protected:
    // Schedule an event to issue all ready nodes in m_inp_node_lst
    void schedIssueReadyNodes();

    // Schedule an event to check for semaphore timeout
    void schedSemaphoreTimeoutEvent();

    /** Helper functions for bit-level operations */

    // Extract bits from position 'pos' with length 'len' from 'val'
    static inline uint64_t getBits(uint64_t val, uint32_t pos, uint32_t len);

    // Set bits at position 'pos' with length 'len' in 'val' to 'set_val'
    static inline uint64_t setBits(uint64_t val,
                                   uint32_t pos,
                                   uint32_t len,
                                   uint64_t set_val);

    // Static vector of all GPUModels in the system
    static std::vector<GPUModel*> s_gpu_model_lst;

    GpuSys* m_sys_p;                              // pointer to the System layer
    Workload* m_workload_p;                       // pointer to workload layer
    Memory* m_memory_p;                           // simulated memory contents
    const int m_id;                               // GPU ID
    const std::string m_name;                     // name for logging
    const enum Memory::MemoryMode m_memory_mode;  // memory simulation mode

    const Tick m_sem_timeout_ticks;  // timeout duration for semaphore acquires
    const timespec_t m_acquire_delay;  // delay complete an acquire

    // Queue of ready-to-issue wrapper nodes
    std::queue<WrapperNode> m_inp_node_lst;

    // True if a semaphore timeout checking event has been scheduled
    bool m_sem_timeout_event_scheduled;

    // Map of semaphore addresses to outstanding semaphores
    std::unordered_map<uint64_t, Semaphore> m_outstanding_semaphores;

    // Map of kernel/node ID to a list of outstanding WGs
    std::unordered_map<int, Kernel> m_outstanding_kernels;

    // Map of barrier IDs to outstanding workgroup barriers
    std::unordered_map<uint32_t, WorkgroupBarrier> m_outstanding_wg_barriers;

    // True if a WG issue event has been scheduled
    bool m_wg_issue_event_scheduled;

    // Protocol of the currently running MSCCL kernel plan
    Msccl::ProtocolType m_protocol;

    // Stats
    Stat::Counter m_stat_num_nodes;
    std::unordered_map<int, Stat::Counter> m_stat_num_node_map;
    Stat::Counter m_stat_num_wgs;
};

}  // namespace AstraSim
