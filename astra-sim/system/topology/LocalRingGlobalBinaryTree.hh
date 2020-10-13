/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __LOCALRINGGLOBALBINARYTREE_HH__
#define __LOCALRINGGLOBALBINARYTREE_HH__

#include <map>
#include <math.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <tuple>
#include <cstdint>
#include <list>
#include <vector>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <assert.h>
#include "astra-sim/system/Common.hh"
#include "RingTopology.hh"
#include "BinaryTree.hh"
#include "ComplexLogicalTopology.hh"

namespace AstraSim{
    class LocalRingGlobalBinaryTree:public ComplexLogicalTopology{
    public:
        RingTopology *local_dimension;
        BinaryTree *global_dimension;
        LocalRingGlobalBinaryTree(int id,BinaryTree::TreeType tree_type,int total_tree_nodes,int start,int stride,int local_dim);
        ~LocalRingGlobalBinaryTree();
    };
}
#endif