// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-network-analytical/common/Type.h>
#include <astra-network-analytical/congestion_aware/Topology.h>
#include <common/CommonNetworkApi.hh>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>

using namespace AstraSim;
using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionAware;

namespace AstraSimAnalyticalCongestionAware {

class CongestionAwareNetworkApi final : public CommonNetworkApi {
  public:
    /**
     * Constructor.
     *
     * @param rank id of the API
     */
    explicit CongestionAwareNetworkApi(int rank) noexcept;

    /**
     * Set the topology to be used. Topology is static object shared by all
     * system's network APIs. Implicit upcast to the CommonNetworkApi's topology
     * of type BaseTopology
     *
     * @param topology_ptr pointer to the to
     */
    static void set_topology(std::shared_ptr<Topology> topology_ptr) noexcept;

    static void periodic_stats(void* args);

    /**
     * Set flag that detailed stats should be used. Initialize detailed stat
     * objects. Begin periodic tracing
     *
     * @param histos_nbins_min_max # bins, min val, and max val for various
     * histograms
     * @param use_detailed_stats whether to use detailed stats
     * @param periodic_stats_period period in ps of periodic stats
     */
    void init_stats(
        const std::vector<std::tuple<int, int, int>>& histos_nbins_min_max,
        bool use_detailed_stats,
        int periodic_stats_period) noexcept override;

    /**
     * Get pointer to an aware type topology. Will be downcast
     *
     * @return (raw) downcasted pointer to (CongestionAware) topology
     */
    Topology* get_aware_topology() noexcept;

    /**
     * Queries topology on resources and/or if able to send. If allowed then
     * calls create_and_schedule_chunk and if not then it queries the topology
     * for the (most critical) resource blocking transmission
     *
     * @param net_req_id unique ID
     * @param count size of message
     * @param net_src source network device
     * @param net_dst destination network device
     *
     * @return resource ID of blocking resource. -1 if nothing blocking (thus
     * successfully transmitted)
     */
    ResourceID try_to_create_and_schedule_chunk(const RequestID net_req_id,
                                                const uint64_t count,
                                                const int net_src,
                                                const int net_dst) override;

    /**
     * Directly schedule chunk transmission with topology send which returns
     * calculated delay between transmission and reception
     *
     * @param net_req_id unique ID
     * @param count size of message
     * @param net_src source network device
     * @param net_dst destination network device
     *
     */
    void create_and_schedule_chunk(const RequestID net_req_id,
                                   const uint64_t count,
                                   const int net_src,
                                   const int net_dst);

    // detailed stats
    static Tick stat_period;

    // detailed stats
    Stat::Histogram stat_histo_router_utilization;
};

}  // namespace AstraSimAnalyticalCongestionAware
