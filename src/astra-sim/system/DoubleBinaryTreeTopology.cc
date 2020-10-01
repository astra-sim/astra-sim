/******************************************************************************
Copyright (c) 2020 Georgia Institute of Technology
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Author : Saeed Rashidi (saeed.rashidi@gatech.edu)
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