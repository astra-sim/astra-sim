// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/common/ReqId.hh>

#include <map>

namespace AstraSim {

namespace {

uint64_t s_request_id_counter = 0;

// Per-key appearance counts, incremented independently by the send side and
// the recv side so the Nth send and the Nth recv on a key land on the same
// registry slot regardless of call order.
std::map<SendRecvKey, int> s_send_counts;
std::map<SendRecvKey, int> s_recv_counts;

// Exactly two lookups ever occur per key (one send, one recv), so the entry
// is erased on the second lookup.
std::map<SendRecvRegistryKey, uint64_t> s_send_recv_registry;

uint64_t claim_send_recv_req_id(const SendRecvRegistryKey& key) {
    auto it = s_send_recv_registry.find(key);
    if (it != s_send_recv_registry.end()) {
        uint64_t id = it->second;
        s_send_recv_registry.erase(it);
        return id;
    }
    uint64_t id = ++s_request_id_counter;
    s_send_recv_registry[key] = id;
    return id;
}

}  // namespace

uint64_t next_request_id() noexcept {
    return ++s_request_id_counter;
}

uint64_t next_send_req_id(const uint32_t tag,
                          const uint32_t srcRank,
                          const uint32_t dstRank,
                          const uint64_t reqSizeNBytes) noexcept {
    const auto n = s_send_counts[{tag, srcRank, dstRank, reqSizeNBytes}]++;
    return claim_send_recv_req_id({tag, srcRank, dstRank, reqSizeNBytes, n});
}

uint64_t next_recv_req_id(const uint32_t tag,
                          const uint32_t srcRank,
                          const uint32_t dstRank,
                          const uint64_t reqSizeNBytes) noexcept {
    const auto n = s_recv_counts[{tag, srcRank, dstRank, reqSizeNBytes}]++;
    return claim_send_recv_req_id({tag, srcRank, dstRank, reqSizeNBytes, n});
}

}  // namespace AstraSim
