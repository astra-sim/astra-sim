#include "astra-sim/workload/CollCommSynchronizer.hh"

#include <sstream>
#include <stdexcept>

#include "astra-sim/common/Logging.hh"
#include "astra-sim/system/CommunicatorGroup.hh"
#include "astra-sim/system/Sys.hh"
#include "astra-sim/workload/HardwareResource.hh"
#include "astra-sim/workload/Workload.hh"

using namespace AstraSim;

std::shared_ptr<CollCommSynchronizer> CollCommSynchronizer::instance = nullptr;

std::shared_ptr<CollCommSynchronizer> CollCommSynchronizer::get_instance(
    Workload* workload) {
    if (instance == nullptr) {
        instance =
            std::shared_ptr<CollCommSynchronizer>(new CollCommSynchronizer());
    }
    instance->workload_map[workload->sys->id] = workload;
    return instance;
}

uint64_t CollCommSynchronizer::hashCollCommNode(
    std::shared_ptr<Chakra::FeederV3::ETFeederNode> node) {
    // simple hash function: comm_type + comm_size + comm_tag
    uint64_t hash = 0;
    hash ^= static_cast<uint64_t>(node->comm_type()) << 32;
    hash ^= static_cast<uint64_t>(node->comm_size()) << 16;
    hash ^= static_cast<uint64_t>(node->comm_tag<uint32_t>(0u));
    return hash;
}

void CollCommSynchronizer::issue_coll_comm(
    std::shared_ptr<Chakra::FeederV3::ETFeederNode> node,
    uint64_t rank,
    CommunicatorGroup* comm_group) {
    uint64_t coll_comm_hash = hashCollCommNode(node);
    if (this->pendingCollComms.find(coll_comm_hash) ==
        this->pendingCollComms.end()) {
        this->pendingCollComms.emplace(coll_comm_hash, CollCommSyncData());
        CollCommSyncData& sync_data = this->pendingCollComms[coll_comm_hash];
        if (comm_group == nullptr) {
            // global communicator
            for (const auto& rank_workload_pair : this->workload_map) {
                sync_data.pending_ranks.insert(rank_workload_pair.first);
            }
        } else {
            for (uint64_t rank : comm_group->involved_NPUs) {
                sync_data.pending_ranks.insert(rank);
            }
        }
    }
    CollCommSyncData& sync_data = this->pendingCollComms.at(coll_comm_hash);
    if (sync_data.coll_comm_nodes.find(rank) !=
        sync_data.coll_comm_nodes.end()) {
        throw std::runtime_error(
            "Rank " + std::to_string(rank) +
            " has already issued collective communication node " +
            std::to_string(node->id()));
    }

    LoggerFactory::get_logger("CollCommSynchronizer")
        ->debug("Rank {} issued collective communication node {}", rank,
                node->id());

    sync_data.coll_comm_nodes[rank] = node;
    sync_data.pending_ranks.erase(rank);
    if (sync_data.pending_ranks.empty()) {
        this->try_dispatch_coll_comm();
    }
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

        LoggerFactory::get_logger("CollCommSynchronizer")
            ->debug("All colelctive ready, dispatching collective "
                    "communication for hash {}",
                    it->first);

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

CollCommSynchronizer::~CollCommSynchronizer() {
    if (this->pendingCollComms.size() != 0) {
        for (auto& hash_syncdata_pair : this->pendingCollComms) {
            uint64_t hash = hash_syncdata_pair.first;
            CollCommSyncData& sync_data = hash_syncdata_pair.second;
            std::ostringstream ss;
            ss << "Pending collective communication hash " << hash
               << " with pending ranks: ";
            for (auto& rank : sync_data.pending_ranks) {
                ss << rank << " ";
            }
            ss << std::endl;
            ss << "Ready ranks: ";
            for (auto& rank_node_pair : sync_data.coll_comm_nodes) {
                ss << rank_node_pair.second->id() << "@" << rank_node_pair.first
                   << " ";
            }
            LoggerFactory::get_logger("CollCommSynchronizer")
                ->critical(ss.str());
        }
    }
}
