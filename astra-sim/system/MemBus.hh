/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __MEMBUS_HH__
#define __MEMBUS_HH__

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
#include "Common.hh"
#include "Callable.hh"

namespace AstraSim{
    class Sys;
    class LogGP;
    class MemBus{
    public:
        enum class Transmition{Fast,Usual};
        LogGP *NPU_side;
        LogGP *MA_side;
        Sys *generator;
        int communication_delay;
        bool model_shared_bus;
        ~MemBus();
        MemBus(std::string side1,std::string side2,Sys *generator,Tick L,Tick o,Tick g,double G,bool model_shared_bus,int communication_delay,bool attach);
        void send_from_NPU_to_MA(Transmition transmition,int bytes,bool processed,bool send_back,Callable *callable);
        void send_from_MA_to_NPU(Transmition transmition,int bytes,bool processed,bool send_back,Callable *callable);
    };
}
#endif