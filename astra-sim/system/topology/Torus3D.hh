/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __TORUS3D_HH__
#define __TORUS3D_HH__

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
#include "RingTopology.hh"

namespace AstraSim{
    class Torus3D:public ComplexLogicalTopology{
    public:
        RingTopology *local_dimension;
        RingTopology *vertical_dimension;
        RingTopology *horizontal_dimension;
        Torus3D(int id,int total_nodes,int local_dim,int vertical_dim);
        ~Torus3D();
    };
}
#endif