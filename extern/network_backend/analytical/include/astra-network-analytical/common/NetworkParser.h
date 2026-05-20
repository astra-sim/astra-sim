/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "common/Type.h"
#include <fstream>
#include <iostream>
#include <json/json.hpp>

namespace NetworkAnalytical {

/**
 * NetworkParser parses the network configuration file in JSON format.
 */
class NetworkParser {
  public:
    /**
     * Constructor.
     *
     * @param path path of the json file
     */
    explicit NetworkParser(const std::string& path) noexcept;

    /**
     * Return the number of network dimensions.
     * Which is calculated by the length of "topology" value
     *
     * @return number of network dimensions
     */
    [[nodiscard]] int get_dims_count() const noexcept;

    /**
     * Read "npus_count" value
     *
     * @return number of NPUs per each demension.
     */
    [[nodiscard]] std::vector<int> get_npus_counts_per_dim() const noexcept;

    /**
     * Read "bandwidth" value
     *
     * @return bandwidth per each dimension
     */
    [[nodiscard]] std::vector<Bandwidth> get_bandwidths_per_dim() const noexcept;

    /**
     * Read "latency" value
     *
     * @return link latency per each dimension
     */
    [[nodiscard]] std::vector<Latency> get_latencies_per_dim() const noexcept;

    /**
     * Read "topology" value and translate it into TopologyBuildingBlock
     * components
     *
     * @return topology building block per each dimension
     */
    [[nodiscard]] std::vector<TopologyBuildingBlock> get_topologies_per_dim() const noexcept;

  private:
    /// number of network dimensions
    int dims_count;

    /// NPUs count per each dimension
    std::vector<int> npus_count_per_dim;

    /// bandwidth per each dimension
    std::vector<Bandwidth> bandwidth_per_dim;

    /// latency per each dimension
    std::vector<Latency> latency_per_dim;

    /// topology building block per each dimension
    std::vector<TopologyBuildingBlock> topology_per_dim;

    /**
     * Parse topology name (in string) into TopologyBuildingBlock enum
     *
     * @param topology_name topology name in string
     *    which can be "Ring", "FullyConnected", or "Switch"
     * @return parsed TopologyBuildingBlock enum class value
     */
    [[nodiscard]] static TopologyBuildingBlock parse_topology_name(const std::string& topology_name) noexcept;

    /**
     * Parse the given JSON object and retrieve network configuration values
     *
     * @param network_config parsed JSON network configuration
     */
    void parse_network_config(const nlohmann::json& network_config) noexcept;

    /**
     * Check the validity and correctness of the parsed network input
     * configurations.
     */
    void check_validity() const noexcept;

    /**
     * Given a json array, read each element and create a std::vector<T>.
     *
     * @tparam T type of the element to be read
     * @param node JSON array to read
     * @return std::vector<T> of read elements
     */
    template <typename T> std::vector<T> parse_vector(const nlohmann::json& node) const noexcept {
        auto parsed_vector = std::vector<T>();

        if (!node.is_array()) {
            std::cerr << "[Error] (network/analytical) expected JSON array" << std::endl;
            std::exit(-1);
        }

        for (const auto& element : node) {
            try {
                parsed_vector.push_back(element.get<T>());
            } catch (const nlohmann::json::exception& e) {
                std::cerr << "[Error] (network/analytical) " << e.what() << std::endl;
                std::exit(-1);
            }
        }

        return parsed_vector;
    }
};

}  // namespace NetworkAnalytical
