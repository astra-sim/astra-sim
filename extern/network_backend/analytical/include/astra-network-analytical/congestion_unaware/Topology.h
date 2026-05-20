/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "common/Type.h"
#include <vector>

using namespace NetworkAnalytical;

namespace NetworkAnalyticalCongestionUnaware {

/**
 * Abstracts a network topology.
 */
class Topology {
  public:
    /**
     * Constructor
     */
    Topology() noexcept;

    /**
     * Estimate the time to be taken to transmit a chunk of size chunk_size
     * from src NPU to dest NPU.
     *
     * @param src src NPU ID
     * @param dest dest NPU ID
     * @param chunk_size size of the chunk to send
     * @return time to send the chunk from src to dest
     */
    [[nodiscard]] virtual EventTime send(DeviceId src, DeviceId dest, ChunkSize chunk_size) const noexcept = 0;

    /**
     * Get the number of NPUs in the topology.
     *
     * @return number of NPUs
     */
    [[nodiscard]] int get_npus_count() const noexcept;

    /**
     * Get the number of network dimensions.
     *
     * @return number of network dimensions
     */
    [[nodiscard]] int get_dims_count() const noexcept;

    /**
     * Get the number of NPUs per dimension.
     *
     * @return number of NPUs per dimension
     */
    [[nodiscard]] std::vector<int> get_npus_count_per_dim() const noexcept;

    /**
     * Get the bandwidth (GB/s) per dimension.
     *
     * @return bandwidth per dimension
     */
    [[nodiscard]] std::vector<Bandwidth> get_bandwidth_per_dim() const noexcept;

  protected:
    /// number of NPUs in the topology
    int npus_count;

    /// number of network dimensions of the topology
    int dims_count;

    /// number of NPUs per each network dimension
    std::vector<int> npus_count_per_dim;

    /// network bandwidth (GB/s) per each network dimension
    std::vector<Bandwidth> bandwidth_per_dim;
};

}  // namespace NetworkAnalyticalCongestionUnaware
