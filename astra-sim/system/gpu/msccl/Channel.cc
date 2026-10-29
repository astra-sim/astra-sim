// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/gpu/msccl/Channel.hh>

#include <astra-sim/common/logging/Logger.hh>

using namespace AstraSim;
using namespace AstraSim::Msccl;

/**
 * MemoryChannel
 */

MemoryChannel::MemoryChannel(int r, uint32_t id) : rank(r), chan_id(id) {
    // Sanity checks
    if (rank < 0) {
        THROW_ERR_NR("Rank in memory channel must be >= 0: rank=%d\n", rank);
    }
}

MemoryChannel::MemoryChannel(const MemoryChannel& other)
    : rank(other.rank),
      chan_id(other.chan_id) {}

MemoryChannel& MemoryChannel::operator=(const MemoryChannel& other) {
    if (this != &other) {
        rank = other.rank;
        chan_id = other.chan_id;
    }
    return *this;
}

bool MemoryChannel::operator==(const MemoryChannel& other) const {
    return (rank == other.rank) && (chan_id == other.chan_id);
}

/**
 * SwitchChannel
 */

SwitchChannel::SwitchChannel(const std::vector<int>& ranks, BufferType t)
    : rank_group(ranks),
      buf_type(t) {
    // Sanity checks
    if (buf_type >= BufferType::NumBufferTypes) {
        THROW_ERR_NR("Invalid buffer type for switch channel: %d\n",
                     static_cast<int>(buf_type));
    }
    if (rank_group.empty()) {
        THROW_ERR_NR("Rank group for switch channel cannot be empty: size=%d\n",
                     rank_group.size());
    }
    for (int r : rank_group) {
        if (r < 0) {
            THROW_ERR_NR("Rank in switch channel must be >= 0: rank=%d\n", r);
        }
    }
}

SwitchChannel::SwitchChannel(const SwitchChannel& other)
    : rank_group(other.rank_group),
      buf_type(other.buf_type) {}

SwitchChannel& SwitchChannel::operator=(const SwitchChannel& other) {
    if (this != &other) {
        rank_group = other.rank_group;
        buf_type = other.buf_type;
    }
    return *this;
}

bool SwitchChannel::operator==(const SwitchChannel& other) const {
    return (rank_group == other.rank_group) && (buf_type == other.buf_type);
}
