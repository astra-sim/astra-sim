/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __MYPACKET_HH__
#define __MYPACKET_HH__

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
    class MyPacket:public Callable
    {
    public:
        int cycles_needed;
        //FIFOMovement *fMovement;
        //FIFO* dest;
        int fm_id;
        int stream_num;
        Callable *notifier;
        Callable *sender;
        int preferred_vnet;
        int preferred_dest;
        int preferred_src;
        Tick ready_time;
        //MyPacket(int cycles_needed, FIFOMovement *fMovement, FIFO *dest);
        MyPacket(int preferred_vnet,int preferred_src ,int preferred_dest);
        void set_notifier(Callable* c);
        void call(EventType event,CallData *data);
        //~MyPacket()= default;
    };
}
#endif