/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __PACKETBUNDLE_HH__
#define __PACKETBUNDLE_HH__

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
#include "MyPacket.hh"
#include "BaseStream.hh"
#include "MemBus.hh"

namespace AstraSim{
    class Sys;
    class PacketBundle:public Callable{
    public:
        std::list<MyPacket*> locked_packets;
        bool processed;
        bool send_back;
        int size;
        Sys *generator;
        BaseStream *stream;
        Tick creation_time;
        MemBus::Transmition transmition;
        PacketBundle(Sys *generator,BaseStream *stream,std::list<MyPacket*> locked_packets, bool processed, bool send_back, int size,MemBus::Transmition transmition);
        PacketBundle(Sys *generator,BaseStream *stream, bool processed, bool send_back, int size,MemBus::Transmition transmition);
        void send_to_MA();
        void send_to_NPU();
        Tick delay;
        void call(EventType event,CallData *data);
        //~PacketBundle()= default;
    };
}
#endif