// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cstdint>
#include <tuple>

namespace AstraSim {

// (tag, srcRank, dstRank, reqSizeNBytes)
using SendRecvKey = std::tuple<uint32_t, uint32_t, uint32_t, uint64_t>;

// (tag, srcRank, dstRank, reqSizeNBytes, occurrence index)
using SendRecvRegistryKey =
    std::tuple<uint32_t, uint32_t, uint32_t, uint64_t, int>;

/**
 * Draw the next value from the single process-wide request-ID sequence.
 *
 * Shared by GPU-model and collective-algorithm requests so IDs never
 * collide in the analytical frontend's static ChunkTracker.
 *
 * @return a freshly allocated, globally unique request ID
 */
uint64_t next_request_id() noexcept;

/**
 * Derive the request ID for a Send, matching what next_recv_req_id() will
 * derive for the corresponding Recv -- both sides compute the same ID from
 * the same inputs, with no communication between them.
 *
 * @param tag stream/tag identifying this transfer
 * @param srcRank rank sending the data
 * @param dstRank rank receiving the data
 * @param reqSizeNBytes size of the transfer in bytes
 *
 * @return the request ID this Send shares with its matching Recv
 */
uint64_t next_send_req_id(uint32_t tag,
                          uint32_t srcRank,
                          uint32_t dstRank,
                          uint64_t reqSizeNBytes) noexcept;

/**
 * Derive the request ID for a Recv, matching what next_send_req_id() derived
 * for the corresponding Send.
 *
 * @param tag stream/tag identifying this transfer
 * @param srcRank rank sending the data
 * @param dstRank rank receiving the data
 * @param reqSizeNBytes size of the transfer in bytes
 *
 * @return the request ID this Recv shares with its matching Send
 */
uint64_t next_recv_req_id(uint32_t tag,
                          uint32_t srcRank,
                          uint32_t dstRank,
                          uint64_t reqSizeNBytes) noexcept;

}  // namespace AstraSim
