/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __RENDEZVOUSSENDDATA_HH__
#define __RENDEZVOUSSENDDATA_HH__

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
#include "BasicEventHandlerData.hh"
#include "SimSendCaller.hh"

namespace AstraSim{
    class Sys;
    class RendezvousSendData:public BasicEventHandlerData,public MetaData{
    public:
        SimSendCaller *send;
        RendezvousSendData(int nodeId,Sys* generator,void *buffer, int count, int type, int dst,
                           int tag, sim_request request, void (*msg_handler)(void *fun_arg), void* fun_arg);
    };
}
#endif