// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>
#include <common/ChunkTracker.hh>
#include <iostream>

using namespace AstraSimAnalytical;

ChunkTracker::ChunkTracker() noexcept {
    // initialize tracker
    tracker = {};
}

// returns base class ptr. Will need dynamic cast
std::optional<ChunkTrackerEntry*> ChunkTracker::search_entry(
    const RequestID req_id) const noexcept {
    assert(req_id >= 0);

    // search entry
    const auto entry = tracker.find(req_id);

    // no entry exists
    if (entry == tracker.end()) {
        return std::nullopt;
    }

    // return pointer to entry
    return entry->second;
}

ChunkTrackerEntry* ChunkTracker::create_new_entry(
    const AstraNetworkAPI::CommandType chunk_type,
    const sim_request& request) noexcept {
    const auto req_id = request.astraReqId;
    assert(req_id >= 0);

    if (chunk_type == AstraNetworkAPI::CommandType::Local_Load) {
        const auto entry =
            tracker.emplace(req_id, new LocalLoadChunkTrackerEntry()).first;
        // return pointer to entry
        return entry->second;
    } else if (chunk_type == AstraNetworkAPI::CommandType::Local_Store) {
        const auto vaddr = request.vaddr;
        const auto entry =
            tracker.emplace(req_id, new LocalStoreChunkTrackerEntry(vaddr))
                .first;
        // return pointer to entry
        return entry->second;
    } else if (chunk_type == AstraNetworkAPI::CommandType::Remote_Load) {
        const auto entry =
            tracker.emplace(req_id, new RemoteLoadChunkTrackerEntry()).first;
        // return pointer to entry
        return entry->second;
    } else if (chunk_type == AstraNetworkAPI::CommandType::Remote_Store) {
        const auto vaddr = request.vaddr;
        const auto entry =
            tracker.emplace(req_id, new RemoteStoreChunkTrackerEntry(vaddr))
                .first;
        // return pointer to entry
        return entry->second;
    } else if (chunk_type == AstraNetworkAPI::CommandType::Send ||
               chunk_type == AstraNetworkAPI::CommandType::Recv) {
        // Send and its matching Recv share req_id (see ReqId.hh); the second
        // call finds what the first one created instead of allocating anew.
        if (auto it = tracker.find(req_id); it != tracker.end()) {
            return it->second;
        }
        const auto entry =
            tracker.emplace(req_id, new SendRecvChunkTrackerEntry()).first;
        return entry->second;
    } else {
        std::cerr << "Unknown ChunkTrackerEntry type! Exiting simulation..."
                  << std::endl;
        exit(-1);
    }

    // never reach here
    return nullptr;
}

void ChunkTracker::pop_entry(const RequestID req_id) noexcept {
    assert(req_id >= 0);

    // find entry
    auto entry = tracker.find(req_id);
    assert(entry != tracker.end());  // entry must exist

    // remove the object from heap
    delete (entry->second);

    // erase entry from the tracker
    tracker.erase(entry);
}
