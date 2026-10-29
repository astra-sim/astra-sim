// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/gpu/msccl/Type.hh>
#include <vector>

namespace AstraSim {
namespace Msccl {

/**
 * Memory Channel
 */
struct MemoryChannel {
    // Rank (GPU ID) to which this channel connects
    int rank;
    // Channel ID within a list of channels connected to the same rank
    uint32_t chan_id;

    MemoryChannel(int r, uint32_t id);
    ~MemoryChannel() = default;
    MemoryChannel(const MemoryChannel& other);
    MemoryChannel& operator=(const MemoryChannel& other);
    bool operator==(const MemoryChannel& other) const;
};

typedef std::reference_wrapper<const MemoryChannel> MemoryChannelRef;

/**
 * Switch Channel
 */
struct SwitchChannel {
    std::vector<int> rank_group;  // Group of ranks connected to this channel
    BufferType buf_type;          // Buffer type used in this channel

    SwitchChannel(const std::vector<int>& ranks, BufferType t);
    ~SwitchChannel() = default;
    SwitchChannel(const SwitchChannel& other);
    SwitchChannel& operator=(const SwitchChannel& other);
    bool operator==(const SwitchChannel& other) const;
};

typedef std::reference_wrapper<const SwitchChannel> SwitchChannelRef;

}  // namespace Msccl
}  // namespace AstraSim
