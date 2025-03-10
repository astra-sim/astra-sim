/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __WORKLOAD_HH__
#define __WORKLOAD_HH__

#include <memory>
#include <string>
#include <unordered_map>

#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/CommunicatorGroup.hh"
#include "astra-sim/workload/HardwareResource.hh"
#include "astra-sim/workload/Statistics.hh"
#include "astra-sim/workload/LocalMemUsageTracker.hh"
#include "extern/graph_frontend/chakra/src/feeder_v3/et_feeder.h"

namespace AstraSim {

class Sys;
class DataSet;

class Workload : public Callable {
  public:
    Workload(Sys* sys,
             std::string et_filename,
             std::string comm_group_filename);
    ~Workload();

    // communicator groups
    void initialize_comm_group(std::string comm_group_filename);
    void issue_pytorch_pg_metadata(
        std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);

    // event-based simulation
    void issue_dep_free_nodes();
    void issue(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void issue_metadata(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void issue_replay(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void issue_remote_mem(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void issue_comp(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void issue_comm(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void issue_coll_comm(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void issue_send_comm(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void issue_recv_comm(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void skip_invalid(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void call(EventType event, CallData* data);
    void fire();

    // stats
    void report();

    Chakra::FeederV3::ETFeeder* et_feeder;
    std::unordered_map<std::string, CommunicatorGroup*> comm_group;
    HardwareResource* hw_resource;
    Sys* sys;
    Statistics* stats;
    std::unique_ptr<LocalMemUsageTracker> local_mem_usage_tracker;
    std::unordered_map<int, uint64_t> collective_comm_node_id_map;
    std::unordered_map<int, DataSet*> collective_comm_wrapper_map;
    bool is_finished;
};

}  // namespace AstraSim

#endif /* __WORKLOAD_HH__ */
