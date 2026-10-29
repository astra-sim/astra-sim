// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>
#include <congestion_unaware/CongestionUnawareNetworkApi.hh>

// for prints
#include <iomanip>
#include <iostream>

using namespace AstraSim;
using namespace AstraSimAnalyticalCongestionUnaware;
using namespace NetworkAnalytical;
// using namespace NetworkAnalyticalCongestionUnaware;

void CongestionUnawareNetworkApi::set_topology(
    std::shared_ptr<Topology> topology_ptr) noexcept {
    assert(topology_ptr != nullptr);

    // move topology
    CommonNetworkApi::topology = std::move(topology_ptr);

    // set topology-related values
    CommonNetworkApi::dims_count = CommonNetworkApi::topology->get_dims_count();
    CommonNetworkApi::bandwidth_per_dim =
        CommonNetworkApi::topology->get_bandwidth_per_dim();
}

CongestionUnawareNetworkApi::CongestionUnawareNetworkApi(
    const int rank) noexcept
    : CommonNetworkApi(rank) {
    assert(rank >= 0);
}

Topology* CongestionUnawareNetworkApi::get_unaware_topology() noexcept {
    auto unaware_topology =
        dynamic_cast<Topology*>(CommonNetworkApi::topology.get());
    assert(unaware_topology);
    return unaware_topology;
}

ResourceID CongestionUnawareNetworkApi::try_to_create_and_schedule_chunk(
    const RequestID net_req_id,
    const uint64_t count,
    const int net_src,
    const int net_dst) {

    DPRINTF("[try_to_create_and_schedule_chunk] Trying send request ID %u of "
            "size %u\n",
            net_req_id, count);

    ResourceID constraining_resource = -1;

    auto unaware_topology = get_unaware_topology();

    // if uncredited then always proceed
    // AND/OR short circuit on self sends (net_src==net_dst)
    if (!unaware_topology->is_ticketed_topology() || net_src == net_dst) {
        auto _delta =
            create_and_schedule_chunk(net_req_id, count, net_src, net_dst);
        // return success (-1)
    }
    // ticket management handled by topology
    else if (unaware_topology->is_send_allowed(net_src, net_dst)) {
        // for unaware, this should be list of length 1 right?
        std::vector<ResourceID> resources_used =
            unaware_topology->claimTickets(net_src, net_dst);

        // net_src, net_dst to send to process_chunk_arrival to determine how to
        // handle
        auto _delta =
            create_and_schedule_chunk(net_req_id, count, net_src, net_dst);

        // track resource usage
        resource_tracker[net_req_id] = resources_used;

        DPRINTF(
            "[try_to_create_and_schedule_chunk] Scheduled over resources%s\n",
            "");
        for (auto _r : resources_used) {
            DPRINTF("[try_to_create_and_schedule_chunk] %d,", _r);
        }
        DPRINTF("%s\n", "");

        // return successfully transmitted AND tracked resource usage. return
        // success (-1)
    } else {
        // IMPORTANT: this needs to be done outside this function
        // as caller will know whether sys or FE will schedule retry
        // let topology decide most critical resource
        constraining_resource =
            unaware_topology->query_constraining_resource(net_src, net_dst);
        assert(constraining_resource >=
               -1);  // -1 is success and >= 0 is valid. Otherwise topology
                     // returned bad resource
    }

    return constraining_resource;
}

timespec_t CongestionUnawareNetworkApi::create_and_schedule_chunk(
    const RequestID net_req_id,
    const uint64_t count,
    const int net_src,
    const int net_dst) {

    auto unaware_topology = get_unaware_topology();

    // create chunk arrival arg
    // create new (uint64_t) RequestID and cast as void ptr. Must delete in
    // process_chunk_arrival
    void* arg_ptr = new RequestID(net_req_id);

    if (net_src == net_dst) {
        // short circuit on self sends (net_src==net_dst) and schedule immediate
        DPRINTF("[create_and_schedule_chunk] Short circuit self send for "
                "request ID %u of size %u\n",
                net_req_id, count);
        // chunk arrival
        const auto immediate_delta = timespec_t({PS, 0});
        sim_schedule(immediate_delta, CommonNetworkApi::process_chunk_arrival,
                     arg_ptr);
        return immediate_delta;
    }

    // compute send communication delay (in AstraSim format)
    const auto delay_ps = unaware_topology->send(net_src, net_dst, count);
    const auto delay = static_cast<Tick>(delay_ps);
    const auto delta = timespec_t({PS, delay});

    DPRINTF("[create_and_schedule_chunk] Scheduled/sent request ID %u of size "
            "%u w/ delay %u\n",
            net_req_id, count, delay);

    // register chunk arrival event after send communication delay
    sim_schedule(delta, CommonNetworkApi::process_chunk_arrival, arg_ptr);

    return delta;
}
