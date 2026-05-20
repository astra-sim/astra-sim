/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "common/Type.h"
#include "congestion_unaware/Topology.h"

using namespace NetworkAnalytical;

namespace NetworkAnalyticalCongestionUnaware {

/**
 * BasicTopology defines 1D topology
 * such as Ring, FullyConnected, and Switch topology,
 * which can be used to construct multi-dimensional topology.
 */
class BasicTopology : public Topology {
  public:
    /**
     * Constructor.
     *
     * @param npus_count number of NPUs in the topology
     * @param bandwidth bandwidth of each link in the topology
     * @param latency latency of each link in the topology
     */
    BasicTopology(int npus_count, Bandwidth bandwidth, Latency latency) noexcept;

    /**
     * Destructor.
     */
    virtual ~BasicTopology() noexcept;

    /**
     * Implement the send method of Topology.
     */
    [[nodiscard]] EventTime send(DeviceId src, DeviceId dest, ChunkSize chunk_size) const noexcept override;

    /**
     * Return the type of the basic topology
     * as a TopologyBuildingBlock enum class element.
     *
     * @return type of the basic topology
     */
    [[nodiscard]] TopologyBuildingBlock get_basic_topology_type() const noexcept;

  protected:
    /**
     * Compute the number of hops between src and dest.
     *
     * @param src src NPU ID
     * @param dest dest NPU ID
     * @return number of hops between src and dest
     */
    [[nodiscard]] virtual int compute_hops_count(DeviceId src, DeviceId dest) const noexcept = 0;

    /// type of the basic topology
    TopologyBuildingBlock basic_topology_type;

  private:
    /**
     * Analytically compute the communication delay.
     *
     * @param hops_count number of hops between src and dest
     * @param chunk_size size of the chunk
     * @return communication delay to send a chunk between src and dest
     */
    [[nodiscard]] EventTime compute_communication_delay(int hops_count, ChunkSize chunk_size) const noexcept;

    /// bandwidth of each link in GB/s
    Bandwidth bandwidth;

    /// bandwidth of each link in B/ns, used for actual computation
    Bandwidth bandwidth_Bpns;

    /// latency of each link in ns
    Latency latency;
};

}  // namespace NetworkAnalyticalCongestionUnaware
