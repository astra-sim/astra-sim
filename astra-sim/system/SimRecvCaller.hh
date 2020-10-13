/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __SIMRECVCALLER_HH__
#define __SIMRECVCALLER_HH__

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
#include "CallData.hh"
#include "Callable.hh"

namespace AstraSim{
    class Sys;
    class SimRecvCaller:public Callable{
    public:
        void *buffer;
        int count;
        int type;
        int src;
        int tag;
        sim_request request;
        void (*msg_handler)(void *fun_arg);
        void* fun_arg;
        void call(EventType type,CallData *data);
        Sys* generator;
        SimRecvCaller(Sys* generator,void *buffer, int count, int type, int src, int tag, sim_request request, void (*msg_handler)(void *fun_arg), void* fun_arg);
    };
}
#endif