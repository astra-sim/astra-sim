#ifndef COLL_COMM_SYNCHRONIZER_HH_
#define COLL_COMM_SYNCHRONIZER_HH_

#include "astra-sim/system/CommunicatorGroup.hh"
#include "extern/graph_frontend/chakra/src/feeder_v3/et_feeder.h"
#include <memory>
#include <stdint.h>
#include <unordered_map>

namespace AstraSim {

class Sys;
class Workload;

class CollCommSynchronizer {
  public:
    constexpr static uint64_t ALLOW_CROSS_SYNC = true;
    static std::shared_ptr<CollCommSynchronizer> get_instance(
        Workload* workload);

    void issue_coll_comm(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node,
                         uint64_t rank,
                         CommunicatorGroup* comm_group);

    void try_dispatch_coll_comm();
    ~CollCommSynchronizer();

  private:
    class CollCommSyncData {
      public:
        std::unordered_map<uint64_t,
                           std::shared_ptr<Chakra::FeederV3::ETFeederNode>>
            coll_comm_nodes;
        std::unordered_set<uint64_t> pending_ranks;
    };
    uint64_t hashCollCommNode(
        std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    std::unordered_map<uint64_t, Workload*> workload_map;
    CollCommSynchronizer() = default;

    std::unordered_map<uint64_t, std::list<CollCommSyncData>> pendingCollComms;

    static std::shared_ptr<CollCommSynchronizer> instance;
};
}  // namespace AstraSim

#endif
