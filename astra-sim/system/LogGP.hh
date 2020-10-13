/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __LOGGP_HH__
#define __LOGGP_HH__

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
#include "MemMovRequest.hh"
#include "MemBus.hh"

namespace AstraSim{
    class Sys;
    class LogGP:public Callable{
    public:
        enum class State{Free,waiting,Sending,Receiving};
        enum class ProcState{Free,Processing};
        int request_num;
        std::string name;
        Tick L;
        Tick o;
        Tick g;
        double G;
        Tick last_trans;
        State curState;
        State prevState;
        ProcState processing_state;
        std::list<MemMovRequest> sends;
        std::list<MemMovRequest> receives;
        std::list<MemMovRequest> processing;

        std::list<MemMovRequest> retirements;
        std::list<MemMovRequest> pre_send;
        std::list<MemMovRequest> pre_process;
        std::list<MemMovRequest>::iterator talking_it;

        LogGP *partner;
        Sys *generator;
        EventType trigger_event;
        int subsequent_reads;
        int THRESHOLD;
        int local_reduction_delay;
        LogGP(std::string name,Sys *generator,Tick L,Tick o,Tick g,double G,EventType trigger_event);
        void process_next_read();
        void request_read(int bytes,bool processed,bool send_back,Callable *callable);
        void switch_to_receiver(MemMovRequest mr,Tick offset);
        void call(EventType event,CallData *data);
        MemBus *NPU_MEM;
        void attach_mem_bus(Sys *generator,Tick L,Tick o,Tick g,double G,bool model_shared_bus,int communication_delay);
        ~LogGP();
    };
}
#endif