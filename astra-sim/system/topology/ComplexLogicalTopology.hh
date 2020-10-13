/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __COMPLEXLOGICALTOPOLOGY_HH__
#define __COMPLEXLOGICALTOPOLOGY_HH__

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
#include "LogicalTopology.hh"

namespace AstraSim{
    class ComplexLogicalTopology: public LogicalTopology{
    public:
        ComplexLogicalTopology(){this->complexity=LogicalTopology::Complexity::Complex;}
        virtual ~ComplexLogicalTopology()=default;
        virtual int get_num_of_dimensions() override{return 1;}
        virtual BasicLogicalTopology* get_basic_topology_at_dimension(int dimension,ComType type) override{return NULL;};
    };
}
#endif