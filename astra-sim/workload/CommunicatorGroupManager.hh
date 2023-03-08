//
// Created by Saeed Rashidi on 3/7/23.
//

#ifndef COMMUNICATORGROUPMANAGER_HH
#define COMMUNICATORGROUPMANAGER_HH
#include <vector>
#include <map>
namespace AstraSim {
class CommunicatorGroup;
class Workload;
class Sys;
class CommunicatorGroupManager {
 public:
  CommunicatorGroupManager(Workload *workload);
  ~CommunicatorGroupManager();
  CommunicatorGroup* get_comm_group(std::vector<int> involved_npus, bool in_switch);

 private:
  static int get_id(std::vector<int> involved_npus,bool in_switch);
    static std::map<std::vector<int>,int> ids_in_switch;
    static std::map<std::vector<int>,int> ids_end_to_end;
    static int index;
    std::map<std::vector<int>,CommunicatorGroup*> in_switch_comm_groups;
    std::map<std::vector<int>,CommunicatorGroup*> end_to_end_comm_groups;
    Sys *sys;


};

}
#endif // ASTRA_SIM_DEV_INTERNAL_COMMUNICATORGROUPMANAGER_HH
