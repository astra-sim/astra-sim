/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
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
#include "astra-sim/system/topology/LogicalTopology.hh"

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