/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "congestion_aware/Switch.h"
#include <cassert>

using namespace NetworkAnalyticalCongestionAware;

Switch::Switch(const int npus_count, const Bandwidth bandwidth, const Latency latency) noexcept
    : BasicTopology(npus_count, npus_count + 1, bandwidth, latency) {
    // e.g., if npus_count=8, then
    // there are total 9 devices, where ordinary npus are 0-7, and switch is 8
    assert(npus_count > 0);
    assert(bandwidth > 0);
    assert(latency >= 0);

    // set switch id
    switch_id = npus_count;

    // connect npus and switches, the link should be bidirectional
    for (auto i = 0; i < npus_count; i++) {
        connect(i, switch_id, bandwidth, latency, true);
    }
}

Route Switch::route(DeviceId src, DeviceId dest) const noexcept {
    // assert npus are in valid range
    assert(0 <= src && src < npus_count);
    assert(0 <= dest && dest < npus_count);

    // construct route
    // start at source, and go to switch, then go to destination
    auto route = Route();
    route.push_back(devices[src]);
    route.push_back(devices[switch_id]);
    route.push_back(devices[dest]);

    return route;
}
