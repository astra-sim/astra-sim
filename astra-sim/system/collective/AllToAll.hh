/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ALLTOALL_HH__
#define __ALLTOALL_HH__

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
#include "Ring.hh"
#include "astra-sim/system/topology/RingTopology.hh"
#include "astra-sim/system/CallData.hh"

namespace AstraSim{
    class AllToAll:public Ring{
    public:
        AllToAll(ComType type,int id,int layer_num,RingTopology *allToAllTopology,int data_size,
                 RingTopology::Direction direction,PacketRouting routing,
                 InjectionPolicy injection_policy,bool boost_mode);
        void run(EventType event,CallData *data);
        void process_max_count();
        int get_non_zero_latency_packets();
    };
}
#endif