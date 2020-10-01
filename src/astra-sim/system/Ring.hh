/******************************************************************************
Copyright (c) 2020 Georgia Institute of Technology
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Author : Saeed Rashidi (saeed.rashidi@gatech.edu)
*******************************************************************************/

#ifndef __RING_HH__
#define __RING_HH__

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
#include "Algorithm.hh"
#include "RingTopology.hh"
#include "MemBus.hh"
#include "MyPacket.hh"

namespace AstraSim{
    class Ring:public Algorithm{
    public:
        //enum class Type{AllReduce,AllGather,ReduceScatter,AllToAll};
        RingTopology::Direction dimension;
        RingTopology::Direction direction;
        MemBus::Transmition transmition;
        int zero_latency_packets;
        int non_zero_latency_packets;
        int id;
        int current_receiver;
        int current_sender;
        int nodes_in_ring;
        int stream_count;
        int max_count;
        int remained_packets_per_max_count;
        int remained_packets_per_message;
        int parallel_reduce;
        PacketRouting routing;
        InjectionPolicy injection_policy;
        std::list<MyPacket> packets;
        bool toggle;
        long free_packets;
        long total_packets_sent;
        long total_packets_received;
        int msg_size;
        std::list<MyPacket*> locked_packets;
        bool processed;
        bool send_back;
        bool NPU_to_MA;

        Ring(ComType type,int id,int layer_num,RingTopology *ring_topology,int data_size, RingTopology::Direction direction,
             PacketRouting routing,InjectionPolicy injection_policy,bool boost_mode);
        virtual void run(EventType event,CallData *data);
        void process_stream_count();
        //void call(EventType event,CallData *data);
        void release_packets();
        virtual void process_max_count();
        void reduce();
        bool iteratable();
        virtual int get_non_zero_latency_packets();
        void insert_packet(Callable *sender);
        bool ready();

        void exit();
    };
}
#endif