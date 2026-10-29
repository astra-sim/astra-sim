// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-network-analytical/congestion_aware/Chunk.h>
#include <cassert>
#include <congestion_aware/CongestionAwareNetworkApi.hh>

// for prints
#include <iomanip>
#include <iostream>

using namespace AstraSim;
using namespace AstraSimAnalyticalCongestionAware;
using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionAware;

Tick CongestionAwareNetworkApi::stat_period = 1000;

void CongestionAwareNetworkApi::set_topology(
    std::shared_ptr<Topology> topology_ptr) noexcept {
    assert(topology_ptr != nullptr);

    // move topology
    CommonNetworkApi::topology = std::move(topology_ptr);

    CommonNetworkApi::dims_count = CommonNetworkApi::topology->get_dims_count();
    CommonNetworkApi::bandwidth_per_dim =
        CommonNetworkApi::topology->get_bandwidth_per_dim();
}

// static
// periodic stat tracking
void CongestionAwareNetworkApi::periodic_stats(void* args) {

    timespec_t current_time;

    // measure all
    for (auto it = sys_to_net_ptr_map.begin(); it != sys_to_net_ptr_map.end();
         ++it) {
        int sys_id = it->first;
        auto common_net_api = it->second;
        auto aware_net_api =
            dynamic_cast<CongestionAwareNetworkApi*>(common_net_api);
        auto topology_ptr = aware_net_api->get_aware_topology();

        current_time = aware_net_api->sim_get_time();

        // TODO
        // std::vector<int> router_occupancy =
        // topology_ptr->calc_router_occupancy(sys_id);
        // aware_net_api->record_router_occupancy(router_occupancy);
    }

    // reschedule period later
    // calculate absolute event time
    const auto event_time =
        current_time.time_val + CongestionAwareNetworkApi::stat_period;
    const auto event_time_ps = static_cast<EventTime>(event_time);

    // if nothing then simulation complete
    if (AstraSim::g_stat_collection.isSimFinished()) {
        return;
    }

    event_queue->schedule_event(
        event_time_ps, CongestionAwareNetworkApi::periodic_stats, nullptr);
}

void CongestionAwareNetworkApi::init_stats(
    const std::vector<std::tuple<int, int, int>>& histos_nbins_min_max,
    bool use_detailed_stats,
    int periodic_stats_period) noexcept {
    assert(CommonNetworkApi::topology != nullptr);

    // simple counter stats
    stat_total_req_ticks.init(this, "total_req_ticks",
                              "Total # ticks for requests for this sys");
    stat_total_req_completed.init(this, "total_req_completed",
                                  "Total # requests for this sys");

    // histogram stats
    // TODO figure best way to pass these params? This is ugly but 12 ints need
    // to be given somehow
    assert(histos_nbins_min_max.size() == 4);
    auto tmp_tuple = histos_nbins_min_max[0];
    stat_histo_noc_mem_req.init(
        this, "histo_noc_mem_req_ticks",
        "Histogram of CU->mem cmd request ticks for this sys",
        std::get<0>(tmp_tuple), std::get<1>(tmp_tuple), std::get<2>(tmp_tuple));
    tmp_tuple = histos_nbins_min_max[1];
    stat_histo_noc_mem_resp.init(
        this, "histo_noc_mem_resp_ticks",
        "Histogram of (local) mem->CU data ticks for this sys",
        std::get<0>(tmp_tuple), std::get<1>(tmp_tuple), std::get<2>(tmp_tuple));
    tmp_tuple = histos_nbins_min_max[2];
    stat_histo_remote_mem_st.init(
        this, "histo_remote_mem_st_ticks",
        "Histogram of CU-> (remote) mem ticks for this sys",
        std::get<0>(tmp_tuple), std::get<1>(tmp_tuple), std::get<2>(tmp_tuple));
    tmp_tuple = histos_nbins_min_max[3];
    stat_histo_remote_mem_ack.init(
        this, "histo_remote_mem_ack_ticks",
        "Histogram of (remote) mem->CU ack ticks for this sys",
        std::get<0>(tmp_tuple), std::get<1>(tmp_tuple), std::get<2>(tmp_tuple));

    detailed_stats = use_detailed_stats;
    if (!use_detailed_stats) {
        return;
    }
    // detailed below

    stat_flow_counter.init(this, "flow_counter",
                           "# packets/messages per flow (src, dest)");
    // stat_flow_latency.init(this,"flow_latency_counter","Cumulative ticks per
    // flow (src, dest)");
    stat_link_usage.init(this, "link_usage",
                         "# packets/messages over a link (i,j)");

    stat_histo_router_utilization.init(
        this, "router_utilization",
        "Histogram of periodically measured queue occupancies", 100, 0, 100);

    // TODO implement periodic stats
    //      currently, function and recalling works
    //      but no stat is tracked
    // CongestionAwareNetworkApi::stat_period = periodic_stats_period;

    // // calculate absolute event time
    // // assume start at 0
    // const auto current_time = timespec_t({PS, 0});
    // const auto event_time =
    //     current_time.time_val + CongestionAwareNetworkApi::stat_period;
    // const auto event_time_ps = static_cast<EventTime>(event_time);

    // event_queue->schedule_event(
    //     event_time_ps, CongestionAwareNetworkApi::periodic_stats, nullptr);
}

CongestionAwareNetworkApi::CongestionAwareNetworkApi(const int rank) noexcept
    : CommonNetworkApi(rank) {
    assert(rank >= 0);
}

Topology* CongestionAwareNetworkApi::get_aware_topology() noexcept {
    auto aware_topology =
        dynamic_cast<Topology*>(CommonNetworkApi::topology.get());
    assert(aware_topology);
    return aware_topology;
}

int CongestionAwareNetworkApi::try_to_create_and_schedule_chunk(
    const RequestID net_req_id,
    const uint64_t count,
    const int net_src,
    const int net_dst) {

    DPRINTF("[try_to_create_and_schedule_chunk] Trying send request ID %u of "
            "size %u\n",
            net_req_id, count);

    int constraining_resource = -1;

    auto aware_topology = get_aware_topology();

    // if uncredited then always proceed
    // AND/OR short circuit on self sends (net_src==net_dst)
    if (!aware_topology->is_ticketed_topology() || net_src == net_dst) {

        create_and_schedule_chunk(net_req_id, count, net_src, net_dst);

        // return success (-1)
        return constraining_resource;
    }

    // ticket management handled by topology
    auto route = aware_topology->route(net_src, net_dst);
    bool can_send = aware_topology->is_send_allowed_on_route(route);

    if (can_send) {

        // for unaware, this should be list of length 1 right?
        std::vector<ResourceID> resources_used =
            aware_topology->claim_tickets_on_route(route);

        // net_src, net_dst to send to process_chunk_arrival to determine how to
        // handle
        create_and_schedule_chunk(net_req_id, count, net_src, net_dst);

        // track resource usage
        resource_tracker[net_req_id] = resources_used;

        DPRINTF(
            "[try_to_create_and_schedule_chunk] Scheduled over resources%s:\n",
            "");
        for (auto _r : resources_used) {
            DPRINTF("[try_to_create_and_schedule_chunk] %d,\n", _r);
        }
        DPRINTF("%s\n", "");

        // return successfully transmitted AND tracked resource usage. return
        // success (-1)
    } else {

        // IMPORTANT: this needs to be done outside this function
        // as caller will know whether sys or FE will schedule retry
        // let topology decide most critical resource
        constraining_resource =
            aware_topology->query_constraining_resource_on_route(route);
        assert(constraining_resource >=
               -1);  // -1 is success and >= 0 is valid. Otherwise topology
                     // returned bad resource
    }

    return constraining_resource;
}

void CongestionAwareNetworkApi::create_and_schedule_chunk(
    const RequestID net_req_id,
    const uint64_t count,
    const int net_src,
    const int net_dst) {

    auto aware_topology = get_aware_topology();

    // create chunk arrival arg
    // create new (uint64_t) RequestID and cast as void ptr. Must delete in
    // process_chunk_arrival
    void* arg_ptr = new RequestID(net_req_id);

    if (net_src == net_dst) {
        DPRINTF("[create_and_schedule_chunk] Short circuit self send for "
                "request_id=%u over network %d -> %d\n",
                net_req_id, net_src, net_dst);

        // short circuit on self sends (net_src==net_dst) and schedule chunk
        // arrival

        // TODO for the self (net_src == net_dst) case then have a constant
        // latency
        const auto immediate_delta = timespec_t({PS, 0});
        sim_schedule(immediate_delta, CommonNetworkApi::process_chunk_arrival,
                     arg_ptr);
        return;
    }

    DPRINTF("[create_and_schedule_chunk] Scheduled/sent for request_id=%u over "
            "network %d -> %d\n",
            net_req_id, net_src, net_dst);
    const auto route = aware_topology->route(net_src, net_dst);
    auto chunk = std::make_unique<Chunk>(
        count, route, CommonNetworkApi::process_chunk_arrival, arg_ptr);
    // initiate transmission from src -> dst.
    aware_topology->send(std::move(chunk));

    if (detailed_stats) {
        stat_flow_counter.incrementValue(net_src, net_dst);
        auto n_hops = route.size() - 1;
        int iter_num = 0;
        for (auto it = route.begin(); std::next(it) != route.end(); ++it) {
            if (iter_num >= n_hops) {
                break;
            }
            auto next_it = std::next(it);
            int hop_src = (*it)->get_id();
            int hop_dst = (*next_it)->get_id();
            stat_link_usage.incrementValue(hop_src, hop_dst);
            iter_num++;
        }
    }
}
