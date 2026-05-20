/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/NetworkParser.h"
#include "congestion_unaware/Helper.h"
#include <iostream>

using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionUnaware;

int main() {
    // Parse network config and create topology
    const auto network_parser = NetworkParser("../input/Ring_FullyConnected_Switch.json");
    const auto topology = construct_topology(network_parser);

    // message settings
    const auto chunk_size = 1'048'576;  // 1 MB

    // Print basic-topology information
    std::cout << "Total NPUs Count: " << topology->get_npus_count() << std::endl;

    // Run sample send-recv
    const auto comm_delay = topology->send(3, 19, chunk_size);
    std::cout << "comm_delay: " << comm_delay << std::endl;

    // terminate
    return 0;
}
