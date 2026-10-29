// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cassert>
#include <string>
#include <unordered_map>

#include <astra-sim/common/Common.hh>

namespace AstraSim {

class AstraNetworkAPI {
  public:
    enum class BackendType { NotSpecified = 0, Garnet, NS3, Analytical };

    enum class CommandType {
        Invalid = 0,
        Send,  // For backward compatibility only (not for XML and JSON plans)
        Recv,  // For backward compatibility only (not for XML and JSON plans)
        Local_Load,
        Local_Store,
        Remote_Load,
        Remote_Store,
        GLdRed,  // in-switch group-load-reduce (e.g., UALink 2.0 specification)
        GSt,     // in-switch group-store (e.g., UALink 2.0 specification)
        FLdRedSt  // in-switch fused load-reduce-store (e.g., UALink 2.0
                  // specification)
    };

    AstraNetworkAPI(int rank) : rank(rank) {};
    virtual ~AstraNetworkAPI() {};

    // Sending a non-sync command to network frontend
    virtual int sim_cmd(CommandType cmd_type,
                        const sim_request& request,
                        void (*msg_handler)(void* fun_arg),
                        void* fun_arg) = 0;

    virtual void sim_schedule(timespec_t delta,
                              void (*fun_ptr)(void* fun_arg),
                              void* fun_arg) = 0;

    virtual BackendType get_backend_type() const {
        return BackendType::NotSpecified;
    };

    virtual int sim_comm_get_rank() const {
        return rank;
    };

    virtual int sim_comm_set_rank(int rank) {
        this->rank = rank;
        return this->rank;
    };

    virtual timespec_t sim_get_time() const = 0;

    virtual double get_BW_at_dimension(int dim) const {
        return -1;
    };

    // Notifies that the workload for this rank has finished. Note that we
    // have one network handler per rank, so a backend overriding this must
    // find its own way to confirm that all ranks have finished their
    // workloads (e.g. ns-3's NS3BackendCompletionTracker).
    virtual void sim_notify_finished() {
        return;
    }

    // Get string name of a given command type
    static std::string getCmdTypeName(CommandType cmd_type);

    // Release a semaphore (supposed to be called by network backend's HBM)
    virtual bool release_semaphore(uint64_t sem_addr);

  protected:
    // Rank ID (GPU ID)
    int rank;

    // func pointer & func arg pointer
    typedef std::pair<void (*)(void*), void*> CallbackFunc;

    // Tracking all in progress callbacks for outstanding network requests
    std::unordered_map<uint64_t, CallbackFunc> m_outstanding_callbacks;

    // Register a new send call's callback
    virtual void register_callback(uint64_t req_id,
                                   const CallbackFunc& callback);

    // Complete a send MPI call
    virtual CallbackFunc retrieve_callback(uint64_t req_id);
};

}  // namespace AstraSim
