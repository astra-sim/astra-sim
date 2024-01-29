/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __COMMUNICATOR_GROUP_HH__
#define __COMMUNICATOR_GROUP_HH__

#include <assert.h>
#include <map>
#include <vector>

#include "astra-sim/system/Common.hh"

namespace AstraSim {

class Sys;
class CollectivePlan;
class CommunicatorGroup {
 public:
  CommunicatorGroup(int id, std::vector<int> involved_NPUs, Sys* generator);
  CollectivePlan* get_collective_plan(ComType comm_type);
  int get_id();
  void set_id(int id);

  // Finds the offset of the node 'sys_id' among the involved NPUs (3th node among involved NPUs, etc.). 
  // Starts with 0. Returns -1 when the given sys_id is not a member of involved_NPUs.
  int get_offset(int sys_id);
  ~CommunicatorGroup();

  std::vector<int> involved_NPUs;
  int num_streams;

 private:
  int id;
  Sys* generator;
  std::map<ComType, CollectivePlan*> comm_plans;
};

} // namespace AstraSim

#endif /* __COMMUNICATOR_GROUP_HH__ */
