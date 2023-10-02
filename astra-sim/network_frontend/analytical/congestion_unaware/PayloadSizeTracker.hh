/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __PAYLOAD_SIZE_TRACKER_HH__
#define __PAYLOAD_SIZE_TRACKER_HH__

#include <vector>

#include "topology/TopologyConfig.hh"

namespace Analytical {
class PayloadSizeTracker {
 public:
  using PayloadSize = TopologyConfig::PayloadSize; // Byte

  PayloadSizeTracker(int dims_count) noexcept;

  void addPayloadSize(PayloadSize payload_size, int dim) noexcept;

  PayloadSize payloadSizeThroughDim(int dim) const noexcept;

  PayloadSize totalPayloadSize() const noexcept;

 private:
  int dims_count;
  std::vector<PayloadSize> payload_size_sent_through_dim;
};
} // namespace Analytical

#endif /* __PAYLOAD_SIZE_TRACKER_HH__ */
