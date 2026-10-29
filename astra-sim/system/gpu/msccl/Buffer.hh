// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/gpu/msccl/Type.hh>
#include <cstdint>
#include <string>
#include <vector>

namespace AstraSim {
namespace Msccl {

/**
 * BufferChunk represents a chunk of data in a specific buffer type on a
 * specific rank (GPU) started with a byte offset and having a certain size
 * in bytes.
 */

struct BufferChunk {
    int rank;  // Rank (GPU ID) where the buffer is located. -1 for INC buffer
    BufferType type;       // Type of the buffer
    uint64_t byte_offset;  // Starting byte offset within the buffer
    uint64_t nbytes;     // Size of this chunk in bytes (aligned to cache line)
    float chunk_factor;  // variable chunk size (only for dynamic_alltoall)
    std::vector<ChannelType>
        access_chan_types;   // Channel types that can access this buffer
    uint32_t data_in_wfreq;  // Data bytes to be accessed per wave to/from this
                             // buffer chunk. For Simple, this is the WFReq
                             // size. For LL, this is the amount of data each
                             // wave will access without the flag threads
    uint32_t
        flag_in_wfreq;  // Flag bytes to be accessed per wave to/from this
                        // buffer chunk. For Simple, this is unused hence = 0.
                        // For LL, this is the amount of data the flag threads
                        // are working on and will only be involved in
                        // read/writes to scratch buffer in packet format

    // Unified constructor for both fixed and variable workload sizing
    // For fixed workload sizing, chunk_factor is 1.0f by default
    // For variable workload sizing, chunk_factor is provided by the user and
    // overrides the default value
    BufferChunk(int rank,
                BufferType type,
                uint64_t byte_offset,
                uint64_t nbytes,
                const std::vector<ChannelType>& access_chan_types,
                uint32_t data_in_wfreq,
                uint32_t flag_in_wfreq,
                float chunk_factor = 1.0f);

    ~BufferChunk() = default;

    BufferChunk(const BufferChunk& other);
    BufferChunk& operator=(const BufferChunk& other);
    bool operator==(const BufferChunk& other) const;
    bool operator!=(const BufferChunk& other) const;

    std::string to_string() const;
    uint64_t getPayloadBytes() const;
};

typedef std::reference_wrapper<const BufferChunk> BufferChunkRef;

}  // namespace Msccl
}  // namespace AstraSim
