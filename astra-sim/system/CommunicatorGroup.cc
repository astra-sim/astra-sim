/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/CommunicatorGroup.hh"

#include <algorithm>

#include "astra-sim/system/Sys.hh"
#include "astra-sim/system/CollectivePlan.hh"

using namespace AstraSim;

CommunicatorGroup::CommunicatorGroup(
    int id,
    std::vector<int> involved_NPUs,
    Sys *generator) {
  set_id(id);
  this->involved_NPUs = involved_NPUs;
  this->generator = generator;
  this->in_switch=false;
  std::sort(involved_NPUs.begin(), involved_NPUs.end());
  assert(std::find(involved_NPUs.begin(), involved_NPUs.end(), generator->id) != involved_NPUs.end());
}
CommunicatorGroup::CommunicatorGroup(
    int id,
    std::vector<int> involved_NPUs,
    bool inSwitch,
    Sys *generator) {
  set_id(id);
  this->involved_NPUs = involved_NPUs;
  this->generator = generator;
  this->in_switch=inSwitch;
  std::sort(involved_NPUs.begin(), involved_NPUs.end());
  assert(std::find(involved_NPUs.begin(), involved_NPUs.end(), generator->id) != involved_NPUs.end());
}

CommunicatorGroup::~CommunicatorGroup(){
  for (auto cg:comm_plans) {
    CollectivePlan* cp = cg.second;
    delete cp;
  }
}

void CommunicatorGroup::set_id(int id){
  assert(id>0);
  this->id = id;
  this->num_streams = id * COMM_GROUP_OFFSET;

}
int CommunicatorGroup::get_id() {
  return id;
}
void CommunicatorGroup::change_id(int new_id) {
  this->num_streams-=(id*COMM_GROUP_OFFSET);
  this->num_streams+=(new_id*COMM_GROUP_OFFSET);
  this->id=new_id;
}

CollectivePlan* CommunicatorGroup::get_collective_plan(ComType comm_type) {
  if (comm_plans.find(comm_type) != comm_plans.end())
    return comm_plans[comm_type];
  if(in_switch){
    assert(generator->total_nodes-1 == involved_NPUs.size());
    std::vector<Sys *> attached_NPUs;
    for(auto npu_id:involved_NPUs){
      attached_NPUs.push_back(generator->all_sys[npu_id]);
    }
    std::vector<Switch *> empty;
    LogicalTopology *logical_topology = new Switch(generator->all_sys[involved_NPUs.size()],attached_NPUs , empty, empty);
    std::vector<CollectiveImpl*> collective_implementation{new CollectiveImpl(CollectiveImplType::InSwitch)};
    std::vector<bool> dimensions_involved(1,true);
    bool should_be_removed = true;
    comm_plans[comm_type] =
        new CollectivePlan(logical_topology, collective_implementation,
                           dimensions_involved, should_be_removed);
    return comm_plans[comm_type];
  }
  else if (generator->total_nodes == involved_NPUs.size()) {
    LogicalTopology *logical_topology = generator->get_logical_topology(comm_type);
    std::vector<CollectiveImpl*> collective_implementation
      = generator->get_collective_implementation(comm_type);
    std::vector<bool> dimensions_involved(10,true);
    bool should_be_removed = false;
    comm_plans[comm_type] =
      new CollectivePlan(logical_topology, collective_implementation,
          dimensions_involved, should_be_removed);
    return comm_plans[comm_type];
  } else {
    LogicalTopology *logical_topology = new RingTopology(RingTopology::Dimension::Local,generator->id,involved_NPUs);
    std::vector<CollectiveImpl*> collective_implementation{new CollectiveImpl(CollectiveImplType::Ring)};
    std::vector<bool> dimensions_involved(1,true);
    bool should_be_removed = true;
    comm_plans[comm_type] =
      new CollectivePlan(logical_topology, collective_implementation,
          dimensions_involved, should_be_removed);
    return comm_plans[comm_type];
  }
}
