//
// Created by Saeed Rashidi on 7/7/22.
//

#include "astra-sim/system/CollectivePlan.hh"

using namespace AstraSim;

CollectivePlan::CollectivePlan(
    LogicalTopology* topology,
    std::vector<CollectiveImpl*> implementation_per_dimension,
    std::vector<bool> dimensions_involved,
    bool should_be_removed) {
  this->topology=topology;
  this->implementation_per_dimension=implementation_per_dimension;
  this->dimensions_involved=dimensions_involved;
  this->should_be_removed=should_be_removed;
}

CollectivePlan::~CollectivePlan() {
  if(should_be_removed){
    delete topology;
    for(auto &ci:implementation_per_dimension)
      delete ci;
  }
}
