// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cmath>
#include <sstream>

#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/system/gpu/msccl/Buffer.hh>
#include <astra-sim/system/gpu/msccl/Type.hh>

using namespace AstraSim;
using namespace AstraSim::Msccl;

BufferChunk::BufferChunk(int r,
                         BufferType t,
                         uint64_t offset,
                         uint64_t size,
                         const std::vector<ChannelType>& chan_types,
                         uint32_t data_in_wfreq,
                         uint32_t flag_in_wfreq,
                         float chunk_factor)
    : rank(r),
      type(t),
      byte_offset(offset),
      nbytes(size),
      chunk_factor(chunk_factor),
      access_chan_types(chan_types),
      data_in_wfreq(data_in_wfreq),
      flag_in_wfreq(flag_in_wfreq) {
    if (rank < -1) {
        THROW_ERR_NR("Rank of a buffer must be >= -1: rank=%d\n", rank);
    }
    if (type >= BufferType::NumBufferTypes) {
        THROW_ERR_NR("Invalid buffer type: %d\n", static_cast<int>(type));
    }
    if (nbytes == 0) {
        THROW_ERR_NR("BufferChunk size must be > 0: nbytes=%d\n", nbytes);
    }
    if (access_chan_types.empty()) {
        THROW_ERR_NR("BufferChunk must have at least one access channel type: "
                     "%d\n",
                     access_chan_types.size());
    }
    if (chunk_factor < 0.0f || chunk_factor > 1.0f) {
        THROW_ERR_NR(
            "chunk_factor must be between 0.0 and 1.0: chunk_factor=%f\n",
            chunk_factor);
    }
}

BufferChunk::BufferChunk(const BufferChunk& other)
    : rank(other.rank),
      type(other.type),
      byte_offset(other.byte_offset),
      nbytes(other.nbytes),
      chunk_factor(other.chunk_factor),
      access_chan_types(other.access_chan_types),
      data_in_wfreq(other.data_in_wfreq),
      flag_in_wfreq(other.flag_in_wfreq) {}

BufferChunk& BufferChunk::operator=(const BufferChunk& other) {
    if (this != &other) {
        rank = other.rank;
        type = other.type;
        byte_offset = other.byte_offset;
        nbytes = other.nbytes;
        chunk_factor = other.chunk_factor;
        access_chan_types = other.access_chan_types;
        data_in_wfreq = other.data_in_wfreq;
        flag_in_wfreq = other.flag_in_wfreq;
    }
    return *this;
}

bool BufferChunk::operator==(const BufferChunk& other) const {
    return (rank == other.rank) && (type == other.type) &&
           (byte_offset == other.byte_offset) && (nbytes == other.nbytes) &&
           (chunk_factor == other.chunk_factor) &&
           (access_chan_types == other.access_chan_types) &&
           (data_in_wfreq == other.data_in_wfreq) &&
           (flag_in_wfreq == other.flag_in_wfreq);
}

bool BufferChunk::operator!=(const BufferChunk& other) const {
    return !(*this == other);
}

std::string BufferChunk::to_string() const {
    std::stringstream ss;
    ss << "{rank=" << rank << ", type=" << bufferTypeToString(type)
       << ", byte_offset=0x" << std::hex << byte_offset << std::dec
       << ", nbytes=" << nbytes << "}";
    return ss.str();
}

uint64_t BufferChunk::getPayloadBytes() const {
    return data_in_wfreq + flag_in_wfreq;
}
