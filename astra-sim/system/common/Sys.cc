// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/common/Sys.hh>

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/common/stats/Statistics.hh>
#include <astra-sim/system/common/BasicEventHandlerData.hh>
#include <astra-sim/system/common/MemEventHandlerData.hh>
#include <astra-sim/system/common/WorkloadLayerHandlerData.hh>
#include <astra-sim/workload/Workload.hh>

#include <json/json.hpp>

using json = nlohmann::json;

namespace AstraSim {
std::vector<Sys*> Sys::all_sys;

/** Builds the state shared by every Sys flavor, including its Workload. */
Sys::Sys(int id,
         std::string workload_configuration,
         std::string comm_group_configuration,
         std::string system_configuration,
         AstraRemoteMemoryAPI* remote_mem,
         AstraNetworkAPI* comm_NI,
         uint64_t comm_size,
         std::string custom_comm_plan_map_filename)
    : m_name("sys" + std::to_string(id)) {

    if ((id + 1) > this->all_sys.size()) {
        this->all_sys.resize(id + 1);
    }
    this->all_sys[id] = this;

    this->id = id;

    this->workload = nullptr;

    this->roofline_enabled = false;
    this->peak_perf = 0;
    this->roofline = nullptr;
    this->local_mem_bw_Bps = 0;

    this->remote_mem = remote_mem;
    if (this->remote_mem) {
        this->remote_mem->set_sys(id, this);
    }

    this->comm_NI = comm_NI;

    this->pending_events = 0;

    if (initializeSys(system_configuration) == false) {
        sysPanic("Unable to initialize the system layer because the file can "
                 "not be openned");
    }

    // Enable DPRINTF logging
    if (this->trace_enabled) {
        g_logger.enable();
    }

    workload =
        new Workload(this, workload_configuration, comm_group_configuration,
                     comm_size, custom_comm_plan_map_filename);

    // Init stats
    m_stat_total_ticks.init(this, "total_ticks", "Total number of ticks");
}

Sys::~Sys() {
    if (roofline_enabled) {
        delete this->roofline;
    }

    all_sys[id] = nullptr;

    if (workload != nullptr) {
        delete workload;
    }

    bool shouldExit = true;
    for (auto& a : all_sys) {
        if (a != nullptr) {
            shouldExit = false;
            break;
        }
    }

    if (shouldExit) {
        exitSimLoop("Exiting");
    }
}

const std::string& Sys::name() const {
    return m_name;
}

Tick Sys::curTick() const {
    return boostedTick();
}

/** Reads the "system-type" key so a frontend can pick which Sys to build. */
std::string Sys::readSystemType(const std::string& filename) {
    std::ifstream in_file{filename};
    if (!in_file) {
        sysPanic("Unable to open system configuration: " + filename);
    }

    const auto config = json::parse(in_file);
    return config.value("system-type", "");
}

bool Sys::initializeSys(std::string filename) {
    std::ifstream inFile;
    inFile.open(filename);
    if (!inFile) {
        if (id == 0) {
            std::cerr << "Unable to open file: " << filename << std::endl;
        }
        exit(1);
    }

    DPRINTF("Initializing system %d\n", this->id);

    json j;
    inFile >> j;

    if (j.contains("peak-perf")) {
        peak_perf = j["peak-perf"];
        peak_perf = peak_perf * 1000000000000;  // TFLOPS
    }
    if (j.contains("local-mem-bw")) {
        local_mem_bw_Bps =
            double(j["local-mem-bw"]) * 1000000000;  // GB/sec -> B/sec
    }
    if (j.contains("roofline-enabled")) {
        if (j["roofline-enabled"] != 0) {
            roofline_enabled = true;
            roofline = new Roofline(local_mem_bw_Bps, peak_perf);
        }
    }
    this->trace_enabled = false;
    if (j.contains("trace-enabled")) {
        if (j["trace-enabled"] != 0) {
            this->trace_enabled = true;
        } else {
            this->trace_enabled = false;
        }
    }
    this->replay_only = false;
    if (j.contains("replay-only")) {
        if (j["replay-only"] != 0) {
            this->replay_only = true;
        } else {
            this->replay_only = false;
        }
    }

    inFile.close();
    return true;
}

Tick Sys::boostedTick() {
    Sys* ts = all_sys[0];
    if (ts == nullptr) {
        for (uint64_t i = 1; i < all_sys.size(); i++) {
            if (all_sys[i] != nullptr) {
                ts = all_sys[i];
                break;
            }
        }
    }
    timespec_t tmp = ts->comm_NI->sim_get_time();  // in ps
    Tick tick = tmp.time_val / CLOCK_PERIOD;
    return tick;
}

void Sys::sysPanic(std::string msg) {
    std::cerr << msg << std::endl;
    exit(1);
}

void Sys::exitSimLoop(std::string msg) {
    std::cout << msg << std::endl;
}

void Sys::call(EventType type, CallData* data) {
    THROW_ERR("Sys::call() is not implemented. sys_id=%d\n", id);
}

void Sys::callEvents() {
    auto cur_tick = Sys::boostedTick();

    // Process all events in the current tick
    while (!event_queue[cur_tick].empty()) {
        assert(pending_events > 0);
        pending_events--;

        auto& callable = event_queue[cur_tick].front();
        (std::get<0>(callable))
            ->call(std::get<1>(callable), std::get<2>(callable));
        event_queue[cur_tick].pop();
    }

    // We're now done with this current tick!
    event_queue.erase(cur_tick);
}

void Sys::register_event(Callable* callable,
                         EventType event,
                         CallData* callData,
                         Tick delta_ticks) {
    auto event_time = Sys::boostedTick() + delta_ticks;

    // Allocate a new event queue if there is no event queue for this timestamp,
    // and schedule an event to process events in the new queue
    if (event_queue.count(event_time) == 0) {
        event_queue.emplace(
            event_time,
            std::queue<std::tuple<Callable*, EventType, CallData*>>());

        timespec_t tmp(time_type_e::PS, delta_ticks);
        BasicEventHandlerData* data =
            new BasicEventHandlerData(id, EventType::CallEvents);
        comm_NI->sim_schedule(tmp, &Sys::handleEvent, data);
    }

    // Push a new event to the queue
    event_queue[event_time].emplace(std::make_tuple(callable, event, callData));

    pending_events++;
    return;
}

void Sys::handleEvent(void* arg) {
    if (arg == nullptr) {
        // A send with no completion handler attached: nothing to do.
        return;
    }

    BasicEventHandlerData* ehd = (BasicEventHandlerData*)arg;
    int id = ehd->sys_id;
    EventType event = ehd->event;

    if (event == EventType::CallEvents) {
        all_sys[id]->callEvents();
        delete ehd;
        return;
    } else if ((event == EventType::NPU_to_MA) ||
               (event == EventType::MA_to_NPU)) {
        all_sys[id]->callEvents();
        return;
    } else if ((event == EventType::CompFinished) ||
               (event == EventType::MemLoadFinished) ||
               (event == EventType::MemStoreFinished)) {
        MemEventHandlerData* mehd = (MemEventHandlerData*)ehd;
        if (mehd->workload) {
            mehd->workload->call(event, mehd->wlhd);
        }
        delete mehd;
        return;
    }

    if (all_sys[id]->claimEvent(event, ehd)) {
        return;
    }

    if (event == EventType::UndefinedEvent) {
        std::cerr << "Undefined event received. Was a semaphore callback "
                  << "invoked more than once when it shouldn't have been?"
                  << std::endl;
        exit(1);
    } else {
        std::cerr << "Error: Unknown event received: " << (int)event
                  << std::endl;
        exit(1);
    }
}

bool Sys::claimEvent([[maybe_unused]] EventType event,
                     [[maybe_unused]] BasicEventHandlerData* ehd) {
    return false;
}

/** Panics on a comm type no derived Sys claimed. */
DataSet* Sys::issueCollective(
    WrapperNode& node,
    [[maybe_unused]] const std::vector<bool>& involved_dim,
    [[maybe_unused]] CommunicatorGroup* comm_group) {
    std::ostringstream message;
    message << "Unknown comm_type=" << node.getCommType()
            << " for node type COMM_COLL_NODE; check that \"system-type\" "
               "matches the workload";
    sysPanic(message.str());
    return nullptr;
}

int Sys::sim_cmd(AstraNetworkAPI::CommandType cmd_type,
                 const sim_request& request,
                 void (*msg_handler)(void* fun_arg),
                 void* fun_arg) {
    return comm_NI->sim_cmd(cmd_type, request, msg_handler, fun_arg);
}

void Sys::updateFinalNumTicks(Tick num_ticks) {
    m_stat_total_ticks += num_ticks;
    g_sim_ticks.sample(num_ticks);
}

}  // namespace AstraSim
