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
#include "extern/graph_frontend/chakra/et_feeder/et_feeder.h"

namespace AstraSim {

class Sys;

class Workload : public Callable {
 public:
  Workload(Sys* sys, std::string eg_filename, std::string comm_group_filename);
  ~Workload();

  // communicator groups
  void initialize_comm_group(std::string comm_group_filename);

  // event-based simulation
  void issue_dep_free_nodes();
  void issue(std::shared_ptr<Chakra::ETFeederNode> node);
  void issue_remote_mem(std::shared_ptr<Chakra::ETFeederNode> node);
  void issue_comp(std::shared_ptr<Chakra::ETFeederNode> node);
  void issue_comm(std::shared_ptr<Chakra::ETFeederNode> node);
  void skip_invalid(std::shared_ptr<Chakra::ETFeederNode> node);
  void call(EventType event, CallData* data);
  void fire();
  void node_sanity_check(std::shared_ptr<Chakra::ETFeederNode> node);

  // stats
  void report();

  Chakra::ETFeeder* et_feeder;
  CommunicatorGroup* comm_group;
  HardwareResource* hw_resource;
  Sys* sys;
  std::unordered_map<int, uint64_t> collective_comm_node_id_map;
  bool is_finished;
};

} // namespace AstraSim

#endif /* __WORKLOAD_HH__ */
