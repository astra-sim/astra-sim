/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "DoubleBinaryTreeTopology.hh"
namespace AstraSim{
    DoubleBinaryTreeTopology::~DoubleBinaryTreeTopology() {
        delete DBMIN;
        delete DBMAX;
    }
    DoubleBinaryTreeTopology::DoubleBinaryTreeTopology(int id,int total_tree_nodes,int start,int stride,int local_dim) {
        std::cout<<"Double binary tree created with total nodes: "<<total_tree_nodes<<" ,start: "<<start<<" ,stride: "<<stride<<std::endl;
        DBMAX=new LocalRingGlobalBinaryTree(id,BinaryTree::TreeType::RootMax,total_tree_nodes,start,stride,local_dim);
        DBMIN=new LocalRingGlobalBinaryTree(id,BinaryTree::TreeType::RootMin,total_tree_nodes,start,stride,local_dim);
        this->counter=0;
    }
    LogicalTopology* DoubleBinaryTreeTopology::get_topology() {
        //return DBMIN;  //uncomment this and comment the rest lines of this funcion if you want to run allreduce only on one logical tree
        LocalRingGlobalBinaryTree *ans=NULL;
        if(counter++==0){
            ans=DBMAX;
        }
        else{
            ans=DBMIN;
        }
        if(counter>1){
            counter=0;
        }
        return ans;
    }
}