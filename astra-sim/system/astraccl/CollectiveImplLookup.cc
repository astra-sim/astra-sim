/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/astraccl/CollectiveImplLookup.hh"

#include <cstdlib>
#include <yaml-cpp/yaml.h>

#include "astra-sim/common/Logging.hh"

using namespace std;
using json = nlohmann::json;

namespace AstraSim {

    CollectiveImplLookup::CollectiveImplLookup(int rank_) : rank(rank_) {}

    // Copied from Sys.cc.
    // TODO: Remove from Sys.cc in next PR
    CollectiveImpl* generate_collective_impl_from_input(
        string collective_impl_str) {
        if (collective_impl_str == "ring") {
            return new CollectiveImpl(CollectiveImplType::Ring);
        } else if (collective_impl_str == "oneRing") {
            return new CollectiveImpl(CollectiveImplType::OneRing);
        } else if (collective_impl_str == "doubleBinaryTree") {
            return new CollectiveImpl(CollectiveImplType::DoubleBinaryTree);
        } else if (collective_impl_str.rfind("direct", 0) == 0) {
            int window = -1;
            if (collective_impl_str != "direct") {
                window = stoi(collective_impl_str.substr(6, 5));
            }
            return new DirectCollectiveImpl(CollectiveImplType::Direct, window);
        } else if (collective_impl_str.rfind("oneDirect", 0) == 0) {
            int window = -1;
            if (collective_impl_str != "oneDirect") {
                window = stoi(collective_impl_str.substr(9, 5));
            }
            return new DirectCollectiveImpl(CollectiveImplType::OneDirect, window);
        } else if (collective_impl_str == "halvingDoubling") {
            return new CollectiveImpl(CollectiveImplType::HalvingDoubling);
        } else if (collective_impl_str == "oneHalvingDoubling") {
            return new CollectiveImpl(CollectiveImplType::OneHalvingDoubling);
        } else {
            auto logger = LoggerFactory::get_logger("astraccl");
            logger->critical("Cannot interpret collective implementations. "
                            "Please check the collective implementations in the sys"
                            "input file");
            exit(1);
        }
    }

    // Copied from Sys.cc.
    // TODO: Remove from Sys.cc in next PR
    CollectiveImpl* generate_custom_collective_impl(
        string chakra_filepath,
        int rank) {
        string filename = chakra_filepath + "." + to_string(rank) + ".et";
        return new CustomCollectiveImpl(CollectiveImplType::CustomCollectiveImpl, filename);
    }

    std::map<int, std::string> parse_per_node_yaml_file(string yaml_filepath) {
        YAML::Node root;
        try {
            root = YAML::LoadFile(yaml_filepath);
        } catch (const YAML::BadFile& e) {
            throw std::runtime_error("Failed to open YAML file: " + yaml_filepath);
        } catch (const YAML::ParserException& e) {
            throw std::runtime_error(std::string("YAML parse error: ") + e.what());
        }

        if (!root || !root.IsMap()) {
            throw std::runtime_error("Top-level YAML must be a mapping of int -> string.");
        }

        std::map<int, std::string> result;
        for (const auto&kv: root) {
            if (!kv.first.IsScalar() || !kv.second.IsScalar()) {
                throw std::runtime_error("YAML mapping keys and values must be scalars.");
            }
            int node_id;
            try {
                node_id = kv.first.as<int>();
            } catch (const YAML::BadConversion& e) {
                throw std::runtime_error("YAML mapping keys must be integers.");
            }
            std::string chakra_filepath = kv.second.as<std::string>();
            result[node_id] = chakra_filepath;
        }

        return result;
    }

    void CollectiveImplLookup::setup_collective_impl_from_config(json j) {
        // This function is intentionally rolled out (multiple if statements
        // instead of for loop across colls) to provide better code clarity.

        // 1. Parse the native collectives. These have the lowest priority.
        if (j.contains("all-reduce-implementation")) {
            vector<string> collective_impl_str_vec = j["all-reduce-implementation"];
            for (auto collective_impl_str : collective_impl_str_vec) {
                CollectiveImpl* ci =
                    generate_collective_impl_from_input(collective_impl_str);
                native_impl_per_coll_dim[ComType::All_Reduce].push_back(ci);
            }
        }
        if (j.contains("reduce-scatter-implementation")) {
            vector<string> collective_impl_str_vec =
                j["reduce-scatter-implementation"];
            for (auto collective_impl_str : collective_impl_str_vec) {
                CollectiveImpl* ci =
                    generate_collective_impl_from_input(collective_impl_str);
                native_impl_per_coll_dim[ComType::Reduce_Scatter].push_back(ci);
            }
        }
        if (j.contains("all-gather-implementation")) {
            vector<string> collective_impl_str_vec = j["all-gather-implementation"];
            for (auto collective_impl_str : collective_impl_str_vec) {
                CollectiveImpl* ci =
                    generate_collective_impl_from_input(collective_impl_str);
                native_impl_per_coll_dim[ComType::All_Gather].push_back(ci);
            }
        }
        if (j.contains("all-to-all-implementation")) {
            vector<string> collective_impl_str_vec = j["all-to-all-implementation"];
            for (auto collective_impl_str : collective_impl_str_vec) {
                CollectiveImpl* ci =
                    generate_collective_impl_from_input(collective_impl_str);
                native_impl_per_coll_dim[ComType::All_to_All].push_back(ci);
            }
        }

        // 2. Parse the custom collectives to be applied on all collectives, if defined.
        // These have the next highest priority.
        if (j.contains("all-to-all-implementation-custom")) {
            vector<string> chakra_filepath_str_vec =
                j["all-to-all-implementation-custom"];
            if (chakra_filepath_str_vec.size() != 1) {
                throw logic_error(
                    "There should be 1 Chakra ET only. In multi-dim collectives, "
                    "that 1 ET file covers all dimensions");
            }
            CollectiveImpl* ci =
                generate_custom_collective_impl(chakra_filepath_str_vec[0], rank);
            global_custom_impl_per_coll[ComType::All_to_All] = ci;
        }
        if (j.contains("all-gather-implementation-custom")) {
            vector<string> chakra_filepath_str_vec =
                j["all-gather-implementation-custom"];
            if (chakra_filepath_str_vec.size() != 1) {
                throw logic_error(
                    "There should be 1 Chakra ET only. In multi-dim collectives, "
                    "that 1 ET file covers all dimensions");
            }
            CollectiveImpl* ci =
                generate_custom_collective_impl(chakra_filepath_str_vec[0], rank);
            global_custom_impl_per_coll[ComType::All_Gather] = ci;
        }
        if (j.contains("reduce-scatter-implementation-custom")) {
            vector<string> chakra_filepath_str_vec =
                j["reduce-scatter-implementation-custom"];
            if (chakra_filepath_str_vec.size() != 1) {
                throw logic_error(
                    "There should be 1 Chakra ET only. In multi-dim collectives, "
                    "that 1 ET file covers all dimensions");
            }
            CollectiveImpl* ci =
                generate_custom_collective_impl(chakra_filepath_str_vec[0], rank);
            global_custom_impl_per_coll[ComType::Reduce_Scatter] = ci;
        }
        if (j.contains("all-reduce-implementation-custom")) {
            vector<string> chakra_filepath_str_vec =
                j["all-reduce-implementation-custom"];
            if (chakra_filepath_str_vec.size() != 1) {
                throw logic_error(
                    "There should be 1 Chakra ET only. In multi-dim collectives, "
                    "that 1 ET file covers all dimensions");
            }
            CollectiveImpl* ci =
                generate_custom_collective_impl(chakra_filepath_str_vec[0], rank);
            global_custom_impl_per_coll[ComType::All_Reduce] = ci;
        }

        // 3. Finally, parse the list of per-chakra-node custom collective algorithm.
        // These have the highest priority.
        if (j.contains("per-node-custom-implementation")) {
            string per_node_custom_impl_filepath = j["per-node-custom-implementation"];
            std::map<int, std::string> per_node_custom_impl_filename =
                parse_per_node_yaml_file(per_node_custom_impl_filepath);

            for (auto const& [node_id, chakra_filepath] : per_node_custom_impl_filename) {
                CollectiveImpl* ci =
                    generate_custom_collective_impl(chakra_filepath, rank);
                per_node_custom_impl[node_id] = ci;
            }
        }
    }

    std::vector<CollectiveImpl*> CollectiveImplLookup::get_collective_impl(
        ComType comm_type,
        uint64_t workload_node_id,
        BypassRule bypass_rule) {

        // Check if there is a per-node custom implementation first.
        if (bypass_rule != BypassRule::BYPASS_PERNODE_CUSTOM &&
            bypass_rule != BypassRule::BYPASS_ALL_CUSTOM) {
            auto it = per_node_custom_impl.find(static_cast<int>(workload_node_id));
            if (it != per_node_custom_impl.end()) {
                return std::vector<CollectiveImpl*>{it->second};
            }
        }

        // Next, check if there is a global custom implementation for this collective type.
        if (bypass_rule != BypassRule::BYPASS_ALL_CUSTOM) {
            auto git = global_custom_impl_per_coll.find(comm_type);
            if (git != global_custom_impl_per_coll.end()) {
                return std::vector<CollectiveImpl*>{git->second};
            }
        }

        // Finally, return the native implementation per dimension.
        auto nit = native_impl_per_coll_dim.find(comm_type);
        if (nit != native_impl_per_coll_dim.end()) {
            return nit->second;
        }

        // Traditional native, it was okay to not define a collective in system input
        // as long as that collective was not actually present.
        // Still, GeneralLogicalTopo was utilized for all collectives.
        if (bypass_rule == BypassRule::BYPASS_ALL_CUSTOM) {
            return {};
        }

        // If no implementation is found, throw an error.
        throw std::runtime_error("No collective implementation found for the given type {" + std::to_string(static_cast<int>(comm_type)) + "} and node ID {" + std::to_string(workload_node_id) + "}.");
    }

} // namespace AstraSim
