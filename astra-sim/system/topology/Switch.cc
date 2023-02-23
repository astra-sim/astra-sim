//
// Created by Saeed Rashidi on 2/21/23.
//

#include "Switch.hh"
namespace AstraSim {

    Switch::Switch(Sys *sw, std::vector<Sys *> attached_NPUs, std::vector<Switch *> parents,std::vector<Switch *> children)
: BasicLogicalTopology(BasicLogicalTopology::BasicTopology::Switch){
  this->sw=sw;
  this->parents=parents;
  this->children=children;
  this->attached_NPUs=attached_NPUs;
}
//Assuming all children attached are NPUs
int Switch::get_num_of_nodes_in_dimension(int dimension) {
        return attached_NPUs.size();
}

}