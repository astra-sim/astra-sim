//
// Created by Saeed Rashidi on 7/7/22.
//

#ifndef __COLLECTIVE_PLAN_H__
#define __COLLECTIVE_PLAN_H__

#include <vector>

#include "astra-sim/system/Common.hh"
#include "astra-sim/system/topology/LogicalTopology.hh"

namespace AstraSim {

class CollectivePlan {
 public:
  LogicalTopology* topology;
  std::vector<CollectiveImpl*> implementation_per_dimension;
  std::vector<bool> dimensions_involved;
  bool should_be_removed;
  CollectivePlan(LogicalTopology* topology,
                 std::vector<CollectiveImpl*> implementation_per_dimension,
                 std::vector<bool> dimensions_involved,
                 bool should_be_removed);
  ~CollectivePlan();
};

}
#endif /* __COLLECTIVE_PLAN_H__ */
