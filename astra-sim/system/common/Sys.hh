// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <chrono>
#include <fstream>
#include <queue>

#include <astra-sim/common/AstraNetworkAPI.hh>
#include <astra-sim/common/AstraRemoteMemoryAPI.hh>
#include <astra-sim/common/AstraSimDataAPI.hh>
#include <astra-sim/common/SimObject.hh>
#include <astra-sim/common/logging/EventTracer.hh>
#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/common/stats/StatCollection.hh>
#include <astra-sim/common/stats/Statistics.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/common/Roofline.hh>

class WrapperNode;

namespace AstraSim {

class BasicEventHandlerData;
class CommunicatorGroup;
class DataSet;
class Workload;

/**
 * Base class for the per-NPU system layer. Holds the event scheduling, the
 * global clock, the NPU registry, and the roofline/workload plumbing that
 * frontends and Workload reach through a base Sys*, regardless of which
 * concrete system model a given NPU runs. A concrete system model derives
 * from this class and supplies the rest through issueCollective() and
 * claimEvent().
 */
class Sys : public Callable, public SimObject {
  public:
    // Constructor / Destructor
    // -------------------------------------------------
    // Takes only the parameters this base class itself consumes. A derived
    // class's own public constructor accepts whatever additional parameters
    // its model needs and forwards the rest here.
    Sys(int id,
        std::string workload_configuration,
        std::string comm_group_configuration,
        std::string system_configuration,
        AstraRemoteMemoryAPI* remote_mem,
        AstraNetworkAPI* comm_NI,
        uint64_t comm_size = 0,
        std::string custom_comm_plan_map_filename = "");
    virtual ~Sys();
    //---------------------------------------------------------------------------

    // Helper Functions
    // ---------------------------------------------------------
    static Tick boostedTick();
    static void sysPanic(std::string msg);
    // Returns the "system-type" value of the system configuration, or "" if
    // the key is absent.
    static std::string readSystemType(const std::string& filename);
    //---------------------------------------------------------------------------

    // Simulation Loop
    // ----------------------------------------------------------
    void exitSimLoop(std::string msg);
    //---------------------------------------------------------------------------

    // General Event Handling
    // ---------------------------------------------------
    void call(EventType type, CallData* data) override;
    void callEvents();
    void register_event(Callable* callable,
                        EventType event,
                        CallData* callData,
                        Tick delta_ticks);
    static void handleEvent(void* arg);
    //---------------------------------------------------------------------------

    // Collective dispatch, driven by Workload::issue_comm on a base Sys*.
    // The default panics on an unsupported comm type; a derived class overrides
    // this to claim the comm types its model implements.
    virtual DataSet* issueCollective(WrapperNode& node,
                                     const std::vector<bool>& involved_dim,
                                     CommunicatorGroup* comm_group);
    //---------------------------------------------------------------------------

    // Low-level Network Primitives
    // ---------------------------------------------
    int sim_cmd(AstraNetworkAPI::CommandType cmd_type,
                const sim_request& request,
                void (*msg_handler)(void* fun_arg),
                void* fun_arg);
    //---------------------------------------------------------------------------

    //---------------------------------------------------------------------------
    // For logging
    //---------------------------------------------------------------------------
    const std::string& name() const override;
    Tick curTick() const override;
    //---------------------------------------------------------------------------

    static std::vector<Sys*> all_sys;  // vector of all Sys objects

    int id;

    // workload
    Workload* workload;

    // roofline model
    bool roofline_enabled;
    double peak_perf;
    Roofline* roofline;
    double local_mem_bw_Bps;

    AstraRemoteMemoryAPI* remote_mem;

    // network
    AstraNetworkAPI* comm_NI;

    uint64_t pending_events;

    std::map<Tick, std::queue<std::tuple<Callable*, EventType, CallData*>>>
        event_queue;

    // statistics
    bool trace_enabled;

    // skip simulation for all nodes and use current duration
    bool replay_only;

    // Update the total number of ticks
    void updateFinalNumTicks(Tick num_ticks);

  protected:
    // Initialization
    // ------------------------------------------------------------
    // Opens system_configuration and parses the keys this base owns. Not
    // virtual: called from the base constructor, where a virtual call cannot
    // reach a derived override because the derived object does not exist
    // yet. A derived class's constructor must independently open the same
    // file to parse its own key slice, after the base is fully constructed.
    bool initializeSys(std::string name);
    //---------------------------------------------------------------------------

    // Event Handling
    // ---------------------------------------------------
    // Offered an event handleEvent's flow-neutral branches did not claim.
    // Returns true if this override claims (and fully handles) the event.
    virtual bool claimEvent(EventType event, BasicEventHandlerData* ehd);
    //---------------------------------------------------------------------------

  private:
    // Name for logging
    const std::string m_name;
    // Stats
    Stat::Counter m_stat_total_ticks;
};

}  // namespace AstraSim
