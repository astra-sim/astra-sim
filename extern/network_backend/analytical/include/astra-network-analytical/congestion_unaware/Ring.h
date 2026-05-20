/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "common/Type.h"
#include "congestion_unaware/BasicTopology.h"

using namespace NetworkAnalytical;

namespace NetworkAnalyticalCongestionUnaware {

/**
 * Implements a ring topology.
 *
 * Ring(8) example:
 * 0 - 1 - 2 - 3
 * |           |
 * 7 - 6 - 5 - 4
 *
 * If ring is uni-directional, then each chunk can flow through:
 * 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 0
 *
 * If the ring is bi-directional, then each chunk can flow through:
 * 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 0
 * 0 <- 1 <- 2 <- 3 <- 4 <- 5 <- 6 <- 7 <- 0
 */
class Ring final : public BasicTopology {
  public:
    /**
     * Constructor
     *
     * @param npus_count number of NPUs in the Ring
     * @param bandwidth bandwidth of each link
     * @param latency latency of each link
     * @param bidirectional whether the ring is bidirectional, defaults to true
     */
    Ring(int npus_count, Bandwidth bandwidth, Latency latency, bool bidirectional = true) noexcept;

  private:
    /**
     * Implements the compute_hops_count method of BasicTopology.
     */
    [[nodiscard]] int compute_hops_count(DeviceId src, DeviceId dest) const noexcept override;

    /// true if the ring is bidirectional, false otherwise
    bool bidirectional;
};

}  // namespace NetworkAnalyticalCongestionUnaware
