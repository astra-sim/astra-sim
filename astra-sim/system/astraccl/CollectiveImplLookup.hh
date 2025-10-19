#ifndef __COLLECTIVEIMPLLOOKUP_HH__
#define __COLLECTIVEIMPLLOOKUP_HH__

#include <json/json.hpp>

#include "astra-sim/common/Common.hh"
#include "astra-sim/system/astraccl/CollectiveImpl.hh"

using json = nlohmann::json;

namespace AstraSim {

// The GeneralComplexTopology class is needed when native algorithm is chosen.
// When *initializing* GeneralComplexTopology, we need to provide only the native algorithms.
// Therefore, set some enumerations to define bypassing rules.
enum class BypassRule {
    NO_BYPASS = 0,
    BYPASS_PERNODE_CUSTOM,  // Bypass priority 1. below. Start with looking at global custom algorithm. No current usecase.
    BYPASS_ALL_CUSTOM, // Bypass priority 1 and 2. below. Look only at native algorithm.
};

/*
 * CollectiveImplLookup decides which algorithm to use for the given collective operation.
 * We use the algorithms in the following order:
 * 1. If the user specifies which custom algorithm to use for a specific operation, use it.
 * 2. If the user specifies a custom algorithm to use for all operations of that collective (e.g. all AllReduce, etc.), use it.
 * 3. Finally, use the native algorithm for that collective specified by the user. (Traditional algorithm specification)
 */

/*
 * TODO: There is currently no safeguard to prevent a case where the custom algorithm's size does not fit the communication group
 * (Whether there is no commgroup specified, defaulting to all ranks, one big commgroup that includes all ranks, or a smaller commgroup).
 */
class CollectiveImplLookup {
    public:
        CollectiveImplLookup(int rank_);

        void setup_collective_impl_from_config(json j);

        // Notes on `workload_node_id`: Ctrl+F for [operation specific custom collective].
        std::vector<CollectiveImpl*> get_collective_impl(
            ComType comm_type,
            uint64_t workload_node_id,
            BypassRule bypass_rule = BypassRule::NO_BYPASS
        );

    private:
        // Map from Chakra node id (integer) to a custom algorithm identifier/filepath.
        // Populated from YAML file. The filename is specified in the system input with the key "per-node-custom-implementation".
        std::map<int, CollectiveImpl*> per_node_custom_impl;

        // Map from Collective Type to a custom algorithm to be applied
        // on all collective of that type, unless overridden by per node configuration.
        // The filename of the Chakra ET for the specific custom algorithm is specified in the system input with the key "all-reduce-implementation-custom".
        std::map<ComType, CollectiveImpl*> global_custom_impl_per_coll;

        // Map from Collective Type to a vector of native algorithms, where
        // each element in the vector corresponds to a dimension.
        // To be applied only if no custom algorithm is specified.
        // The algorithm for each dimension is specified in the system input with the key "all-reduce-implementation".
        std::map<ComType, std::vector<CollectiveImpl*>> native_impl_per_coll_dim;

        int rank;
};

} // namespace AstraSim

#endif /* __COLLECTIVEIMPLLOOKUP_HH__ */