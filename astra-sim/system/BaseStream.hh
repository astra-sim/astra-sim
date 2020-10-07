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

#ifndef __BASESTREAM_HH__
#define __BASESTREAM_HH__

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
#include "StreamStat.hh"
#include "CollectivePhase.hh"
#include "DataSet.hh"
#include "Sys.hh"
#include "src/astra-sim/system/topology/LogicalTopology.hh"

namespace AstraSim{
    class RecvPacketEventHadndlerData;
    class BaseStream:public Callable,public  StreamStat{
    public:
        static std::map<int,int> synchronizer;
        static std::map<int,int> ready_counter;
        static std::map<int,std::list<BaseStream *>> suspended_streams;
        virtual ~BaseStream()=default;
        int stream_num;
        int total_packets_sent;
        SchedulingPolicy preferred_scheduling;
        std::list<CollectivePhase> phases_to_go;
        int current_queue_id;
        CollectivePhase my_current_phase;
        ComType current_com_type;
        Tick creation_time;
        Sys *owner;
        DataSet *dataset;
        int steps_finished;
        int initial_data_size;
        int priority;
        StreamState state;
        bool initialized;

        Tick last_phase_change;

        int test;
        int test2;
        uint64_t phase_latencies[10];

        void changeState(StreamState state);
        virtual void consume(RecvPacketEventHadndlerData *message)=0;
        virtual void init()=0;

        BaseStream(int stream_num,Sys *owner,std::list<CollectivePhase> phases_to_go);
        void declare_ready();
        bool is_ready();
        void consume_ready();
        void suspend_ready();
        void resume_ready(int st_num);
        void destruct_ready();

    };
}
#endif