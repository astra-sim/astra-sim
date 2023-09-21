/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "PayloadSizeTracker.hh"

using namespace Analytical;

PayloadSizeTracker::PayloadSizeTracker(int dims_count) noexcept
    : dims_count(dims_count) {
  payload_size_sent_through_dim = std::vector<PayloadSize>(dims_count, 0);
}

void PayloadSizeTracker::addPayloadSize(
    PayloadSize payload_size,
    int dim) noexcept {
  payload_size_sent_through_dim[dim] += payload_size;
}

PayloadSizeTracker::PayloadSize PayloadSizeTracker::payloadSizeThroughDim(
    int dim) const noexcept {
  return payload_size_sent_through_dim[dim];
}

PayloadSizeTracker::PayloadSize PayloadSizeTracker::totalPayloadSize()
    const noexcept {
  PayloadSize total_payload = 0;
  for (const auto payload : payload_size_sent_through_dim) {
    total_payload += payload;
  }
  return total_payload;
}
