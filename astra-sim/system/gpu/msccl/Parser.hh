// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cassert>
#include <json/json.hpp>
#include <unordered_map>
#include <vector>

#include <astra-sim/system/gpu/GPUModel.hh>
#include <astra-sim/system/gpu/GpuOp.hh>
#include <astra-sim/system/gpu/Memory.hh>
#include <astra-sim/system/gpu/msccl/Buffer.hh>
#include <astra-sim/system/gpu/msccl/Channel.hh>
#include <astra-sim/system/gpu/msccl/Semaphore.hh>
#include <astra-sim/system/gpu/msccl/Type.hh>

using json = nlohmann::json;

namespace AstraSim {
namespace Msccl {

// Per-packet data/flag byte counts; chunk scale is (data + flag) / data.
struct PacketLayout {
    uint32_t data_nbytes;
    uint32_t flag_nbytes;
};

static constexpr PacketLayout LL_PACKET{8, 8};
static constexpr PacketLayout LL64_PACKET{56, 8};
static constexpr PacketLayout LL128_PACKET{120, 8};
static constexpr PacketLayout LL256_PACKET{248, 8};

inline uint64_t alignUpToCacheLine(uint64_t nbytes) {
    const uint64_t rem = nbytes & GPUModel::CLINE_ADDR_MASK;
    return rem == 0 ? nbytes : nbytes + (GPUModel::CLINE_NBYTES - rem);
}

struct PlanChunkLayout {
    uint64_t nbytes;         // chunk size in bytes (aligned to cache line)
    uint32_t data_in_wfreq;  // data bytes to be accessed per wave
    uint32_t flag_in_wfreq;  // flag bytes to be accessed per wave
};

// Per-chunk byte size and per-wave access fields from protocol + buffer type.
// Only Scratch is expanded for LL/LL128 packet layout; Input/Output keep
// base_chunk_nbytes (cache-line aligned).
inline PlanChunkLayout planChunkNbytes(uint64_t base_chunk_nbytes,
                                       ProtocolType protocol,
                                       BufferType buf_type,
                                       uint32_t wfreq_nbytes) {
    uint64_t nbytes = alignUpToCacheLine(base_chunk_nbytes);
    const PacketLayout* const pkt =
        protocol == ProtocolType::LL
            ? &LL_PACKET
            : (protocol == ProtocolType::LL64
                   ? &LL64_PACKET
                   : (protocol == ProtocolType::LL128
                          ? &LL128_PACKET
                          : (protocol == ProtocolType::LL256 ? &LL256_PACKET
                                                             : nullptr)));
    uint32_t data_in_wfreq = wfreq_nbytes;
    uint32_t flag_in_wfreq = 0;
    if (pkt != nullptr) {
        const uint32_t pkt_nbytes = pkt->data_nbytes + pkt->flag_nbytes;
        assert(wfreq_nbytes % pkt_nbytes == 0);
        data_in_wfreq = wfreq_nbytes * pkt->data_nbytes / pkt_nbytes;
        if (buf_type == BufferType::Scratch) {
            flag_in_wfreq = wfreq_nbytes * pkt->flag_nbytes / pkt_nbytes;
            assert(data_in_wfreq + flag_in_wfreq == wfreq_nbytes);
            nbytes = alignUpToCacheLine(static_cast<uint64_t>(
                base_chunk_nbytes * static_cast<float>(pkt_nbytes) /
                pkt->data_nbytes));
        }
    }

    return {nbytes, data_in_wfreq, flag_in_wfreq};
}

struct GpuParser;

/**
 * ThreadblockParser encapsulates the parsing of threadblock-level MSCCL++ plan
 * from JSON file.
 */
struct ThreadblockParser {
    // Reference to the parent GPU parser
    const GpuParser& gpu_parser;
    // Threadblock ID
    uint32_t tb_id;
    // TB-level memory channels
    std::vector<MemoryChannelRef> tb_mem_chan_lst;
    // TB-level switch channels
    std::vector<SwitchChannelRef> tb_swt_chan_lst;
    // TB-level remote buffers
    std::vector<BufferChunkRef> tb_rem_buf_lst;
    // Sequence of GpuOps for this threadblock
    std::vector<GpuOpPtr> op_lst;

    // Constructor & destructor
    ThreadblockParser(const GpuParser& _gpu_parser,
                      uint32_t _tb_id,
                      uint32_t unit_chunk_cnt,
                      const json& j_tb);
    ~ThreadblockParser() = default;

  private:
    // Parse TB-level channel lists from JSON to generate GpuOp
    GpuOpPtr parseOp(const json& j_op,
                     uint64_t niters,
                     uint64_t iter_nbytes,
                     uint32_t unit_chunk_cnt);

    // Parse buffer chunk from JSON
    BufferChunk parseBufferChunk(const json& j_buf_chunk,
                                 uint32_t unit_chunk_cnt);
};

/**
 * GpuParser encapsulates the parsing of GPU-level MSCCL++ plan from JSON file.
 */
struct GpuParser {
    // Protocol information
    ProtocolType protocol;
    // GPU ID
    const uint32_t gpu_id;
    // Loop unrolling factor (i.e., number of data elements processed at a time)
    uint32_t unrolling_factor;
    // Buffer chunk size in bytes
    const uint64_t chunk_nbytes;
    // Wavefront request size in bytes
    const uint32_t wfreq_nbytes;
    // Number of threads per threadblock/workgroup
    uint32_t num_threads_per_tb;
    // Per-GPU remote buffers
    std::vector<Msccl::BufferChunk> rem_buf_lst;
    // The expected memory contents of each buffer
    std::unordered_map<uint32_t, const MemoryCell*> expected_memory;
    // Per-GPU memory channels
    std::vector<Msccl::MemoryChannel> mem_chan_lst;
    // Per-GPU switch channels
    std::vector<Msccl::SwitchChannel> swt_chan_lst;
    // Per-GPU semaphores
    std::vector<Msccl::Semaphore> sema_lst;
    // Number of input/output/scratch chunks
    std::unordered_map<Msccl::BufferType, uint32_t> num_buf_chunks;
    // List of threadblock parsers, one for each threadblock
    std::vector<ThreadblockParser> tb_parsers;
    // Parse unit_chunk_cnt, if present in the plan, to support variable chunk
    // transfer sizes
    uint32_t unit_chunk_cnt;
    // Whether or not the collective is in-place
    bool inplace;

    // Constructor & destructor
    GpuParser(const std::string& json_file,
              uint32_t _gpu_id,
              uint32_t _unrolling_factor,
              uint64_t _chunk_nbytes,
              uint32_t _wfreq_nbytes);
    ~GpuParser() = default;

    // Return the number of threadblocks
    uint32_t getNumThreadblocks() const;

    // Return the first GpuOp for a given threadblock ID
    GpuOpPtr getFirstGpuOp(uint32_t tb_id) const;

  private:
    // Parse channel information from JSON
    void parseChannels(const json& j_chan_info);

    // Parse remote buffer information from JSON
    void parseRemoteBuffers(const json& j_buf_info);

    // Parse local (intra-GPU) semaphore information from JSON
    void parseLocalSemaphores(const json& j_sema_info);

    // Parse threadblock information from JSON
    void parseThreadblock(const json& j_tb_info);

    // Parse the expected memory contents
    void parseMemory(const json& j_buffer);
};

}  // namespace Msccl
}  // namespace AstraSim
