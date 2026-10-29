// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/StreamBaseline.hh>

#include <astra-sim/system/simple/SimpleSys.hh>
#include <astra-sim/system/simple/astraccl/Algorithm.hh>

using namespace AstraSim;

StreamBaseline::StreamBaseline(SimpleSys* owner,
                               DataSet* dataset,
                               int stream_id,
                               std::list<CollectivePhase> phases_to_go,
                               int priority)
    : BaseStream(stream_id, owner, phases_to_go) {
    this->owner = owner;
    this->stream_id = stream_id;
    this->phases_to_go = phases_to_go;
    this->dataset = dataset;
    this->priority = priority;
    steps_finished = 0;
    initial_data_size = phases_to_go.front().initial_data_size;
}

void StreamBaseline::init() {
    initialized = true;
    last_init = Sys::boostedTick();
    if (!my_current_phase.enabled) {
        return;
    }
    my_current_phase.algorithm->run(EventType::StreamInit, nullptr);
    if (steps_finished == 1) {
        queuing_delay.push_back(last_phase_change - creation_time);
    }
    queuing_delay.push_back(Sys::boostedTick() - last_phase_change);
    total_packets_sent = 1;
}

void StreamBaseline::call(EventType event, CallData* data) {
    // callData in IssueWFReq is gbl SIMDid
    // TODO - better way so that we can avoid this if cond
    SharedBusStat* sharedBusStat = nullptr;
    if ((event != EventType::DispatchInflightWFReq) &&
        (event != EventType::LocalReqDispatch) &&
        (event != EventType::IssueWFReq) && (event != EventType::PacketTxAck)) {
        sharedBusStat = (SharedBusStat*)data;
        update_bus_stats(BusType::Both, sharedBusStat);
    }

    if ((event == EventType::IssueWFReq) ||
        (event == EventType::LocalReqDispatch) ||
        (event == EventType::DispatchInflightWFReq) ||
        (event == EventType::PacketTxAck)) {
        my_current_phase.algorithm->run(event, data);
    } else {
        // TODO: Why is the event hard-coded to General ? why not use event arg
        my_current_phase.algorithm->run(EventType::General, data);
    }
    if (sharedBusStat != nullptr) {
        delete sharedBusStat;
    }
}

void StreamBaseline::consume(RecvPacketEventHandlerData* message) {
    net_message_latency.back() +=
        Sys::boostedTick() - message->ready_time;  // not accurate
    net_message_counter++;
    my_current_phase.algorithm->run(EventType::PacketReceived, message);
}
