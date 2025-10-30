#include "astra-sim/workload/CollCommSynchronizer.hh"
#include "astra-sim/system/CommunicatorGroup.hh"
#include "astra-sim/system/Sys.hh"
#include "astra-sim/workload/HardwareResource.hh"
#include "astra-sim/workload/Workload.hh"

using namespace AstraSim;

CollCommSynchronizer* CollCommSynchronizer::instance = nullptr;

CollCommSynchronizer* CollCommSynchronizer::get_instance(Workload* workload) {
    if (instance == nullptr) {
        instance = new CollCommSynchronizer();
    }
    instance->workload_map[workload->sys->id] = workload;
    return instance;
}

void CollCommSynchronizer::issue_coll_comm(
    std::shared_ptr<Chakra::FeederV3::ETFeederNode> node,
    uint64_t rank,
    CommunicatorGroup* comm_group) {
    uint64_t coll_comm_hash = hashCollCommNode(node);
    if (this->pendingCollComms.find(coll_comm_hash) ==
        this->pendingCollComms.end()) {
        CollCommSyncData sync_data;
        for (uint64_t rank : comm_group->involved_NPUs) {
            sync_data.pending_ranks.insert(rank);
        }
        this->pendingCollComms[coll_comm_hash] = sync_data;
    }
    CollCommSyncData& sync_data = this->pendingCollComms[coll_comm_hash];
    if (sync_data.coll_comm_nodes.find(rank) !=
        sync_data.coll_comm_nodes.end()) {
        throw std::runtime_error(
            "Rank " + std::to_string(rank) +
            " has already issued collective communication node " +
            std::to_string(node->id()));
    }

    sync_data.coll_comm_nodes[rank] = node;
    sync_data.pending_ranks.erase(rank);
}

void CollCommSynchronizer::try_dispatch_coll_comm() {
    for (auto it = this->pendingCollComms.begin();
         it != this->pendingCollComms.end();) {
        CollCommSyncData& sync_data = it->second;
        if (!sync_data.pending_ranks.empty()) {
            ++it;
            continue;
        }

        // all ranks are ready, check if all hw resource empty
        bool all_available = true;
        Workload* workload = nullptr;
        for (auto& rank_node_pair : sync_data.coll_comm_nodes) {
            uint64_t rank = rank_node_pair.first;
            auto node = rank_node_pair.second;
            workload = this->workload_map[rank];
            if (!workload->hw_resource->is_available(node)) {
                all_available = false;
                break;
            }
        }
        if (!all_available) {
            ++it;
            continue;
        }

        // all ranks are ready and all hw resource are available, dispatch
        for (auto& rank_node_pair : sync_data.coll_comm_nodes) {
            uint64_t rank = rank_node_pair.first;
            auto node = rank_node_pair.second;
            workload = this->workload_map[rank];
            workload->dispatch_coll_comm(node);
        }
        it = this->pendingCollComms.erase(it);
    }
}
