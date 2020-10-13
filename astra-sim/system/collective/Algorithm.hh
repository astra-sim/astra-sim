/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ALGORITHM_HH__
#define __ALGORITHM_HH__

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
#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/BaseStream.hh"
#include "astra-sim/system/CallData.hh"
#include "astra-sim/system/topology/LogicalTopology.hh"

namespace AstraSim{
    class Algorithm:public Callable{
    public:
        enum class Name{Ring,DoubleBinaryTree,AllToAll};
        Name name;
        int id;
        BaseStream *stream;
        LogicalTopology *logicalTopology;
        int data_size;
        int final_data_size;
        ComType comType;
        bool enabled;
        int layer_num;
        Algorithm(int layer_num);
        virtual ~Algorithm()= default;
        virtual void run(EventType event,CallData *data)=0;
        virtual void exit();
        virtual void init(BaseStream *stream);
        virtual void call(EventType event,CallData *data);
    };
}
#endif