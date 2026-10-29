// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>
#include <common/ChunkTrackerEntry.hh>
#include <iostream>

using namespace AstraSim;
using namespace NetworkAnalytical;
using namespace AstraSimAnalytical;

ChunkTrackerEntry::ChunkTrackerEntry() noexcept
    : send_request(),
      current_state(AllChunkStates::Invalid) {}

const analytical_network_request* ChunkTrackerEntry::get_send_request()
    const noexcept {
    return &send_request;
}

void ChunkTrackerEntry::set_send_request_from_sim_request(
    const sim_request* sim_req, const Sys_ID src_id) noexcept {
    assert(sim_req);
    // copyish constructor
    send_request = analytical_network_request(sim_req);
    send_request.srcRank = src_id;
}

// basic version. can be overridden
bool ChunkTrackerEntry::is_completed() const noexcept {
    if (current_state == AllChunkStates::Completed) {
        return true;
    }
    return false;
}

AllChunkStates ChunkTrackerEntry::get_current_state() const noexcept {
    return current_state;
}

Sys_ID ChunkTrackerEntry::get_original_system_id() const noexcept {
    return send_request.srcRank;
}

void ChunkTrackerEntry::set_transmission_tick(Tick val) noexcept {
    send_tick = val;
}

Tick ChunkTrackerEntry::get_transmission_tick() const noexcept {
    return send_tick;
}

LocalLoadChunkTrackerEntry::LocalLoadChunkTrackerEntry() : ChunkTrackerEntry() {
    // initialize state
    current_state = AllChunkStates::SrcCUToSrcBuf_ReqCmd;
}

void LocalLoadChunkTrackerEntry::transition_next_state() noexcept {

    switch (current_state) {
    case AllChunkStates::SrcCUToSrcBuf_ReqCmd:
        current_state = AllChunkStates::SrcBufToSrcCU_Data;
        break;
    case AllChunkStates::SrcBufToSrcCU_Data:
        current_state = AllChunkStates::Completed;
        break;
    default:
        std::cerr << "Unknown/illegal chunk state in transition switch case"
                  << std::endl;
        assert(0);  // error out for now
        break;
    }
}

LocalStoreChunkTrackerEntry::LocalStoreChunkTrackerEntry(const uint64_t vaddr)
    : ChunkTrackerEntry(),
      _vaddr(vaddr) {
    // initialize state
    current_state = AllChunkStates::SrcCUToSrcBuf_Data;
}

void LocalStoreChunkTrackerEntry::transition_next_state() noexcept {

    switch (current_state) {
    case AllChunkStates::SrcCUToSrcBuf_Data:
        current_state = AllChunkStates::SrcBufToSrcCU_AckCtrl;
        break;
    case AllChunkStates::SrcBufToSrcCU_AckCtrl:
        current_state = AllChunkStates::Completed;
        break;
    default:
        std::cerr << "Unknown/illegal chunk state in transition switch case"
                  << std::endl;
        assert(0);  // error out for now
        break;
    }
}

uint64_t LocalStoreChunkTrackerEntry::vaddr() const noexcept {
    return _vaddr;
}

RemoteLoadChunkTrackerEntry::RemoteLoadChunkTrackerEntry()
    : ChunkTrackerEntry() {
    // initialize state
    current_state = AllChunkStates::SrcCUToDstBuf_ReqCmd;
}

void RemoteLoadChunkTrackerEntry::transition_next_state() noexcept {

    switch (current_state) {
    case AllChunkStates::SrcCUToDstBuf_ReqCmd:
        current_state = AllChunkStates::DstBufToSrcCU_Data;
        break;
    case AllChunkStates::DstBufToSrcCU_Data:
        current_state = AllChunkStates::Completed;
        break;
    default:
        std::cerr << "Unknown/illegal chunk state in transition switch case"
                  << std::endl;
        assert(0);  // error out for now
        break;
    }
}

RemoteStoreChunkTrackerEntry::RemoteStoreChunkTrackerEntry(const uint64_t vaddr)
    : ChunkTrackerEntry(),
      _vaddr(vaddr) {
    // initialize state
    current_state = AllChunkStates::SrcCUToDstBuf_Data;
}

void RemoteStoreChunkTrackerEntry::transition_next_state() noexcept {

    switch (current_state) {
    case AllChunkStates::SrcCUToDstBuf_Data:
        current_state = AllChunkStates::DstBufToSrcCU_AckCtrl;
        break;
    case AllChunkStates::DstBufToSrcCU_AckCtrl:
        current_state = AllChunkStates::Completed;
        break;
    default:
        std::cerr << "Unknown/illegal chunk state in transition switch case"
                  << std::endl;
        assert(0);  // error out for now
        break;
    }
}

uint64_t RemoteStoreChunkTrackerEntry::vaddr() const noexcept {
    return _vaddr;
}

SendRecvChunkTrackerEntry::SendRecvChunkTrackerEntry() noexcept
    : ChunkTrackerEntry(),
      _recv_posted(false) {
    // nothing to run until the Send side fills in the transfer
    current_state = AllChunkStates::Unstarted;
}

void SendRecvChunkTrackerEntry::transition_next_state() noexcept {

    switch (current_state) {
    case AllChunkStates::Unstarted:
        current_state = AllChunkStates::SrcToDstGPU_Data;
        break;
    case AllChunkStates::SrcToDstGPU_Data:
        current_state = AllChunkStates::Completed;
        break;
    default:
        std::cerr << "Unknown/illegal chunk state in transition switch case"
                  << std::endl;
        assert(0);  // error out for now
        break;
    }
}

void SendRecvChunkTrackerEntry::mark_recv_posted() noexcept {
    _recv_posted = true;
}

bool SendRecvChunkTrackerEntry::is_recv_posted() const noexcept {
    return _recv_posted;
}
