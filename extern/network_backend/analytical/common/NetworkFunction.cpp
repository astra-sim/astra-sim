/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/NetworkFunction.h"
#include <cassert>

using namespace NetworkAnalytical;

Bandwidth NetworkAnalytical::bw_GBps_to_Bpns(const Bandwidth bw_GBps) noexcept {
    assert(bw_GBps > 0);

    // 1 GB is 2^30 B
    // 1 s is 10^9 ns
    return bw_GBps * (1 << 30) / (1'000'000'000);  // GB/s to B/ns
}
