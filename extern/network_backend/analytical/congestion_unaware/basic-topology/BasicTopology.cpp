/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "congestion_unaware/BasicTopology.h"
#include "common/NetworkFunction.h"
#include <cassert>

using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionUnaware;

BasicTopology::BasicTopology(const int npus_count, const Bandwidth bandwidth, const Latency latency) noexcept
    : latency(latency),
      basic_topology_type(TopologyBuildingBlock::Undefined),
      Topology() {
    assert(npus_count > 0);
    assert(bandwidth > 0);
    assert(latency >= 0);

    // set topology shape
    this->npus_count = npus_count;
    npus_count_per_dim.push_back(npus_count);
    dims_count = 1;
    this->bandwidth = bandwidth;
    bandwidth_per_dim.push_back(bandwidth);

    // translate bandwidth from GB/s to B/ns
    bandwidth_Bpns = bw_GBps_to_Bpns(bandwidth);
}

// default destructor
BasicTopology::~BasicTopology() noexcept = default;

EventTime BasicTopology::send(const DeviceId src, const DeviceId dest, const ChunkSize chunk_size) const noexcept {
    assert(0 <= src && src < npus_count);
    assert(0 <= dest && dest < npus_count);
    assert(src != dest);
    assert(chunk_size > 0);

    // get hops count
    auto hops_count = compute_hops_count(src, dest);

    // return communication delay
    return compute_communication_delay(hops_count, chunk_size);
}

EventTime BasicTopology::compute_communication_delay(const int hops_count, const ChunkSize chunk_size) const noexcept {
    assert(hops_count > 0);
    assert(chunk_size > 0);

    // compute link delay and serialization delay
    auto link_delay = hops_count * latency;
    auto serialization_delay = static_cast<double>(chunk_size) / bandwidth_Bpns;

    // comms_delay is the summation of the two
    auto comms_delay = link_delay + serialization_delay;

    // return EventTime type of comms_delay
    return static_cast<EventTime>(comms_delay);
}

TopologyBuildingBlock BasicTopology::get_basic_topology_type() const noexcept {
    assert(basic_topology_type != TopologyBuildingBlock::Undefined);

    return basic_topology_type;
}
