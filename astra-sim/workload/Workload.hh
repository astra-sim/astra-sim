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
#include "extern/graph_frontend/chakra/eg_feeder/eg_feeder.h"

namespace AstraSim {

class Sys;
class CommunicatorGroupManager;

class Workload : Callable {
 public:
  Workload(
      Sys* sys,
      std::string eg_filename,
      std::string comm_group_filename);
  ~Workload();

  // event-based simulation
  void issue_dep_free_nodes();
  void issue(std::shared_ptr<Chakra::EGFeederNode> node);
  void issue_comp(std::shared_ptr<Chakra::EGFeederNode> node);
  void issue_comm(std::shared_ptr<Chakra::EGFeederNode> node);
  void skip_invalid(std::shared_ptr<Chakra::EGFeederNode> node);
  void call(EventType event, CallData* data);
  void fire();

  // stats
  void report();
  bool file_exists (const std::string& name);
  Chakra::EGFeeder* eg_feeder;
  CommunicatorGroupManager *comm_group_manager;
  HardwareResource* hw_resource;
  Sys* sys;
  std::map<int, uint64_t> collective_comm_node_id_map;
  bool is_finished;
};

}

#endif /* __WORKLOAD_HH__ */
