//
// Created by Saeed Rashidi on 3/7/23.
//

#include "astra-sim/workload/CommunicatorGroupManager.hh"
#include "astra-sim/workload/Workload.hh"
#include "astra-sim/system/CommunicatorGroup.hh"

namespace AstraSim {
std::map<std::vector<int>,int> CommunicatorGroupManager::ids_in_switch;
std::map<std::vector<int>,int> CommunicatorGroupManager::ids_end_to_end;
int CommunicatorGroupManager::index=1;

int CommunicatorGroupManager::get_id(std::vector<int> involved_npus,bool in_switch) {
  if(in_switch){
    if(ids_in_switch.find(involved_npus)==ids_in_switch.end())
      ids_in_switch[involved_npus]=index++;
    return ids_in_switch[involved_npus];
  } else{
    if(ids_end_to_end.find(involved_npus)==ids_end_to_end.end())
      ids_end_to_end[involved_npus]=index++;
    return ids_end_to_end[involved_npus];
  }
}

CommunicatorGroupManager::CommunicatorGroupManager(Workload* workload) {
  this->sys=workload->sys;
}
CommunicatorGroup* CommunicatorGroupManager::get_comm_group(std::vector<int> involved_npus, bool in_switch) {
  if(involved_npus.size()==0)
    return nullptr;
  std::sort(involved_npus.begin(),involved_npus.end());
  int id=get_id(involved_npus,in_switch);

  if(in_switch){
    CommunicatorGroup *cgroup= nullptr;
    if(in_switch_comm_groups.find(involved_npus)!=in_switch_comm_groups.end()){
      cgroup=in_switch_comm_groups[involved_npus];
      if(cgroup->get_id()!=id){
        cgroup->change_id(id);
      }
      return cgroup;
    }
    else{
      cgroup= new CommunicatorGroup(id, involved_npus, in_switch, sys);
      in_switch_comm_groups[involved_npus]=cgroup;
      return cgroup;
    }
  } else{
    CommunicatorGroup *cgroup= nullptr;
    if(end_to_end_comm_groups.find(involved_npus)!=end_to_end_comm_groups.end()){
      cgroup=end_to_end_comm_groups[involved_npus];
      if(cgroup->get_id()!=id){
        cgroup->change_id(id);
      }
      return cgroup;
    }
    else{
      cgroup= new CommunicatorGroup(id, involved_npus, in_switch, sys);
      end_to_end_comm_groups[involved_npus]=cgroup;
      return cgroup;
    }
  }
}
CommunicatorGroupManager::~CommunicatorGroupManager() {
  for(auto &cg:in_switch_comm_groups){
    delete cg.second;
  }
  for(auto &cg:end_to_end_comm_groups){
    delete cg.second;
  }
}

}