//
// Created by Saeed Rashidi on 2/21/23.
//

#ifndef ASTRA_SIM_DEV_INTERNAL_Switch_H
#define ASTRA_SIM_DEV_INTERNAL_Switch_H
#include <vector>
#include "astra-sim/system/topology/BasicLogicalTopology.hh"

namespace AstraSim {
class Sys;
class Switch: public BasicLogicalTopology{
 public:
    Sys *sw; //switch pointer
    std::vector<Switch *> parents;
    std::vector<Switch *> children;
    std::vector<Sys *> attached_NPUs;
    Switch (Sys *sw, std::vector<Sys *> attached_NPUs, std::vector<Switch *> parents,std::vector<Switch *> children);
    virtual int get_num_of_nodes_in_dimension(int dimension) override;
};

}
#endif // ASTRA_SIM_DEV_INTERNAL_Switch_H
