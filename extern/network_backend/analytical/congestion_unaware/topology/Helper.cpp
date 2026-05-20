/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "congestion_unaware/Helper.h"
#include "congestion_unaware/BasicTopology.h"
#include "congestion_unaware/FullyConnected.h"
#include "congestion_unaware/MultiDimTopology.h"
#include "congestion_unaware/Ring.h"
#include "congestion_unaware/Switch.h"
#include <cstdlib>
#include <iostream>

using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionUnaware;

std::shared_ptr<Topology> NetworkAnalyticalCongestionUnaware::construct_topology(
    const NetworkParser& network_parser) noexcept {
    // get network_parser info
    const auto dims_count = network_parser.get_dims_count();
    const auto topologies_per_dim = network_parser.get_topologies_per_dim();
    const auto npus_counts_per_dim = network_parser.get_npus_counts_per_dim();
    const auto bandwidths_per_dim = network_parser.get_bandwidths_per_dim();
    const auto latencies_per_dim = network_parser.get_latencies_per_dim();

    // if dims_count is 1, just create basic topology
    if (dims_count == 1) {
        // retrieve basic topology info
        const auto topology_type = topologies_per_dim[0];
        const auto npus_count = npus_counts_per_dim[0];
        const auto bandwidth = bandwidths_per_dim[0];
        const auto latency = latencies_per_dim[0];

        // create and return basic topology
        switch (topology_type) {
        case TopologyBuildingBlock::Ring:
            return std::make_shared<Ring>(npus_count, bandwidth, latency);
        case TopologyBuildingBlock::Switch:
            return std::make_shared<Switch>(npus_count, bandwidth, latency);
        case TopologyBuildingBlock::FullyConnected:
            return std::make_shared<FullyConnected>(npus_count, bandwidth, latency);
        default:
            // shouldn't reach here
            std::cerr << "[Error] (network/analytical/congestion_unaware)" << "Not supported topology" << std::endl;
            std::exit(-1);
        }
    }

    // otherwise, create multi-dim basic-topology
    const auto multi_dim_topology = std::make_shared<MultiDimTopology>();

    // create and append dims
    for (auto dim = 0; dim < dims_count; dim++) {
        // retrieve info
        const auto topology_type = topologies_per_dim[dim];
        const auto npus_count = npus_counts_per_dim[dim];
        const auto bandwidth = bandwidths_per_dim[dim];
        const auto latency = latencies_per_dim[dim];

        // create a network dim
        std::unique_ptr<BasicTopology> dim_topology;
        switch (topology_type) {
        case TopologyBuildingBlock::Ring:
            dim_topology = std::make_unique<Ring>(npus_count, bandwidth, latency);
            break;
        case TopologyBuildingBlock::Switch:
            dim_topology = std::make_unique<Switch>(npus_count, bandwidth, latency);
            break;
        case TopologyBuildingBlock::FullyConnected:
            dim_topology = std::make_unique<FullyConnected>(npus_count, bandwidth, latency);
            break;
        default:
            // shouldn't reach here
            std::cerr << "[Error] (network/analytical/congestion_unaware)" << "Not supported basic-topology"
                      << std::endl;
            std::exit(-1);
        }

        // append network dimension
        multi_dim_topology->append_dimension(std::move(dim_topology));
    }

    // return created multi-dimensional topology
    return multi_dim_topology;
}
