// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-network-analytical/common/Type.h>
#include <astra-network-analytical/congestion_unaware/Topology.h>
#include <astra-sim/common/Common.hh>
#include <common/CommonNetworkApi.hh>
#include <cstring>
#include <iostream>
#include <vector>

using namespace AstraSim;
using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;
// using namespace NetworkAnalyticalCongestionUnaware;

namespace AstraSimAnalyticalCongestionUnaware {

/**
 * CongestionUnawareNetworkApi is a AstraNetworkAPI
 * implemented for congestion_unaware analytical network backend.
 */
class CongestionUnawareNetworkApi final : public CommonNetworkApi {
  public:
    /**
     * Constructor.
     *
     * @param rank id of the API
     */
    explicit CongestionUnawareNetworkApi(int rank) noexcept;

    /**
     * Set the topology to be used. Topology is static object shared by all
     * system's network APIs. Implicit upcast to the CommonNetworkApi's topology
     * of type BaseTopology
     *
     * @param topology_ptr pointer to the to
     */
    static void set_topology(std::shared_ptr<Topology> topology_ptr) noexcept;

    /**
     * Get pointer to an unaware type topology. Will be downcast
     *
     * @return (raw) downcasted pointer to (CongestionAware) topology
     */
    Topology* get_unaware_topology() noexcept;

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
     * @return Expected/known delay for chunk transmission
     */
    timespec_t create_and_schedule_chunk(const RequestID net_req_id,
                                         const uint64_t count,
                                         const int net_src,
                                         const int net_dst);
};

}  // namespace AstraSimAnalyticalCongestionUnaware
