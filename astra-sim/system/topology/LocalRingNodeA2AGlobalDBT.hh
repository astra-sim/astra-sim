/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __LOCALRINGNODEA2AGLOBALDBT_HH__
#define __LOCALRINGNODEA2AGLOBALDBT_HH__

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
#include "DoubleBinaryTreeTopology.hh"

namespace AstraSim{
    class LocalRingNodeA2AGlobalDBT:public ComplexLogicalTopology{
    public:
        DoubleBinaryTreeTopology *global_all_reduce_dimension;
        LocalRingNodeA2AGlobalDBT(int id,int total_tree_nodes,int start,int stride,int local_dim);
        ~LocalRingNodeA2AGlobalDBT();
    };
}
#endif