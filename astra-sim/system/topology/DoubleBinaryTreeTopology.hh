/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __DOUBLEBINARYTREETOPOLOGY_HH__
#define __DOUBLEBINARYTREETOPOLOGY_HH__

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
#include "ComplexLogicalTopology.hh"
#include "LocalRingGlobalBinaryTree.hh"
namespace AstraSim{
    class DoubleBinaryTreeTopology:public ComplexLogicalTopology{
    public:
        int counter;
        LocalRingGlobalBinaryTree *DBMAX;
        LocalRingGlobalBinaryTree *DBMIN;
        LogicalTopology *get_topology();
        ~DoubleBinaryTreeTopology();
        DoubleBinaryTreeTopology(int id,int total_tree_nodes,int start,int stride,int local_dim);
    };
}
#endif