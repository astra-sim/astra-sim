/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "common/Type.h"

namespace NetworkAnalytical {

/**
 * Convert bandwidth from GB/s to B/ns.
 * Here, 1 GB = 2^30 B
 * 1 s = 10^9 ns
 *
 * @param bw_GBps bandwidth in GB/s
 * @return translated bandwidth in B/ns
 */
Bandwidth bw_GBps_to_Bpns(Bandwidth bw_GBps) noexcept;

}  // namespace NetworkAnalytical
