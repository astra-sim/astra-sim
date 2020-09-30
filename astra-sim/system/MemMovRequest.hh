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

#ifndef __MEMMOVREQUEST_HH__
#define __MEMMOVREQUEST_HH__

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
#include "SharedBusStat.hh"

namespace AstraSim{
    class Sys;
    class LogGP;
    class MemMovRequest:public Callable,public SharedBusStat{
    public:
        static int id;
        int my_id;
        int size;
        int latency;
        Callable *callable;
        bool processed;
        bool send_back;
        bool mem_bus_finished;
        Sys *generator;
        EventType callEvent=EventType::General;
        LogGP *loggp;
        std::list<MemMovRequest>::iterator pointer;
        void call(EventType event,CallData *data);

        Tick total_transfer_queue_time;
        Tick total_transfer_time;
        Tick total_processing_queue_time;
        Tick total_processing_time;
        Tick start_time;
        int request_num;
        MemMovRequest(int request_num,Sys *generator,LogGP *loggp,int size,int latency,Callable *callable,bool processed,bool send_back);
        void wait_wait_for_mem_bus(std::list<MemMovRequest>::iterator pointer);
        void set_iterator(std::list<MemMovRequest>::iterator pointer){
            this->pointer=pointer;
        }
        //~MemMovRequest()= default;
    };
}
#endif