/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "common/ChunkIdGeneratorEntry.hh"
#include <astra-network-analytical/common/Type.h>
#include <map>
#include <tuple>

using namespace NetworkAnalytical;

namespace AstraSimAnalytical {

/**
 * ChunkIdGenerator generates unique chunk id for sim_send() and sim_recv()
 * calls given (tag, src, dest, chunk_size) tuple.
 */
class ChunkIdGenerator {
  public:
    /// Key = (tag, src, dest, chunk_size)
    using Key = std::tuple<int, int, int, ChunkSize>;

    /**
     * Constructor.
     */
    ChunkIdGenerator() noexcept;

    /**
     * Create unique chunk id for sim_send() call.
     *
     * @param tag tag of the sim_send() call
     * @param src src NPU ID of the sim_send() call
     * @param dest dest NPU ID of the sim_send() call
     * @param chunk_size chunk size of the sim_send() call
     * @return unique sim_send() chunk id
     */
    [[nodiscard]] int create_send_chunk_id(int tag,
                                           int src,
                                           int dest,
                                           ChunkSize chunk_size) noexcept;

    /**
     * Create unique chunk id for sim_recv call.
     *
     * @param tag tag of the sim_recv call
     * @param src src NPU ID of the sim_recv call
     * @param dest dest NPU ID of the sim_recv call
     * @param chunk_size chunk size of the sim_recv call
     * @return unique sim_recv chunk id
     */
    [[nodiscard]] int create_recv_chunk_id(int tag,
                                           int src,
                                           int dest,
                                           ChunkSize chunk_size) noexcept;

  private:
    /// map from (tag, src, dest, chunk_size) tuple to ChunkIdGeneratorEntry
    std::map<Key, ChunkIdGeneratorEntry> chunk_id_map;
};

}  // namespace AstraSimAnalytical
