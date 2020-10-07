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

#ifndef __SHAREDBUSSTAT_HH__
#define __SHAREDBUSSTAT_HH__

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
#include "CallData.hh"

namespace AstraSim{
class SharedBusStat:public CallData{
public:
    double total_shared_bus_transfer_queue_delay;
    double total_shared_bus_transfer_delay;
    double total_shared_bus_processing_queue_delay;
    double total_shared_bus_processing_delay;

    double total_mem_bus_transfer_queue_delay;
    double total_mem_bus_transfer_delay;
    double total_mem_bus_processing_queue_delay;
    double total_mem_bus_processing_delay;
    int mem_request_counter;
    int shared_request_counter;
    SharedBusStat(BusType busType,double total_bus_transfer_queue_delay,double total_bus_transfer_delay,
                  double total_bus_processing_queue_delay,double total_bus_processing_delay){
        if(busType==BusType::Shared){
            this->total_shared_bus_transfer_queue_delay=total_bus_transfer_queue_delay;
            this->total_shared_bus_transfer_delay=total_bus_transfer_delay;
            this->total_shared_bus_processing_queue_delay=total_bus_processing_queue_delay;
            this->total_shared_bus_processing_delay=total_bus_processing_delay;

            this->total_mem_bus_transfer_queue_delay=0;
            this->total_mem_bus_transfer_delay=0;
            this->total_mem_bus_processing_queue_delay=0;
            this->total_mem_bus_processing_delay=0;
        }
        else{
            this->total_shared_bus_transfer_queue_delay=0;
            this->total_shared_bus_transfer_delay=0;
            this->total_shared_bus_processing_queue_delay=0;
            this->total_shared_bus_processing_delay=0;

            this->total_mem_bus_transfer_queue_delay=total_bus_transfer_queue_delay;
            this->total_mem_bus_transfer_delay=total_bus_transfer_delay;
            this->total_mem_bus_processing_queue_delay=total_bus_processing_queue_delay;
            this->total_mem_bus_processing_delay=total_bus_processing_delay;
        }
        shared_request_counter=0;
        mem_request_counter=0;
    }
    void update_bus_stats(BusType busType,SharedBusStat *sharedBusStat){
        if(busType==BusType::Shared){
            total_shared_bus_transfer_queue_delay+=sharedBusStat->total_shared_bus_transfer_queue_delay;
            total_shared_bus_transfer_delay+=sharedBusStat->total_shared_bus_transfer_delay;
            total_shared_bus_processing_queue_delay+=sharedBusStat->total_shared_bus_processing_queue_delay;
            total_shared_bus_processing_delay+=sharedBusStat->total_shared_bus_processing_delay;
            shared_request_counter++;
        }
        else if(busType==BusType::Mem){
            total_mem_bus_transfer_queue_delay+=sharedBusStat->total_mem_bus_transfer_queue_delay;
            total_mem_bus_transfer_delay+=sharedBusStat->total_mem_bus_transfer_delay;
            total_mem_bus_processing_queue_delay+=sharedBusStat->total_mem_bus_processing_queue_delay;
            total_mem_bus_processing_delay+=sharedBusStat->total_mem_bus_processing_delay;
            mem_request_counter++;
        }
        else{
            total_shared_bus_transfer_queue_delay+=sharedBusStat->total_shared_bus_transfer_queue_delay;
            total_shared_bus_transfer_delay+=sharedBusStat->total_shared_bus_transfer_delay;
            total_shared_bus_processing_queue_delay+=sharedBusStat->total_shared_bus_processing_queue_delay;
            total_shared_bus_processing_delay+=sharedBusStat->total_shared_bus_processing_delay;
            total_mem_bus_transfer_queue_delay+=sharedBusStat->total_mem_bus_transfer_queue_delay;
            total_mem_bus_transfer_delay+=sharedBusStat->total_mem_bus_transfer_delay;
            total_mem_bus_processing_queue_delay+=sharedBusStat->total_mem_bus_processing_queue_delay;
            total_mem_bus_processing_delay+=sharedBusStat->total_mem_bus_processing_delay;
            shared_request_counter++;
            mem_request_counter++;
        }
    }
    void update_bus_stats(BusType busType,SharedBusStat sharedBusStat){
        if(busType==BusType::Shared){
            total_shared_bus_transfer_queue_delay+=sharedBusStat.total_shared_bus_transfer_queue_delay;
            total_shared_bus_transfer_delay+=sharedBusStat.total_shared_bus_transfer_delay;
            total_shared_bus_processing_queue_delay+=sharedBusStat.total_shared_bus_processing_queue_delay;
            total_shared_bus_processing_delay+=sharedBusStat.total_shared_bus_processing_delay;
            shared_request_counter++;
        }
        else if(busType==BusType::Mem){
            total_mem_bus_transfer_queue_delay+=sharedBusStat.total_mem_bus_transfer_queue_delay;
            total_mem_bus_transfer_delay+=sharedBusStat.total_mem_bus_transfer_delay;
            total_mem_bus_processing_queue_delay+=sharedBusStat.total_mem_bus_processing_queue_delay;
            total_mem_bus_processing_delay+=sharedBusStat.total_mem_bus_processing_delay;
            mem_request_counter++;
        }
        else{
            total_shared_bus_transfer_queue_delay+=sharedBusStat.total_shared_bus_transfer_queue_delay;
            total_shared_bus_transfer_delay+=sharedBusStat.total_shared_bus_transfer_delay;
            total_shared_bus_processing_queue_delay+=sharedBusStat.total_shared_bus_processing_queue_delay;
            total_shared_bus_processing_delay+=sharedBusStat.total_shared_bus_processing_delay;
            total_mem_bus_transfer_queue_delay+=sharedBusStat.total_mem_bus_transfer_queue_delay;
            total_mem_bus_transfer_delay+=sharedBusStat.total_mem_bus_transfer_delay;
            total_mem_bus_processing_queue_delay+=sharedBusStat.total_mem_bus_processing_queue_delay;
            total_mem_bus_processing_delay+=sharedBusStat.total_mem_bus_processing_delay;
            shared_request_counter++;
            mem_request_counter++;
        }
    }
    void take_bus_stats_average(){
        total_shared_bus_transfer_queue_delay/=shared_request_counter;
        total_shared_bus_transfer_delay/=shared_request_counter;
        total_shared_bus_processing_queue_delay/=shared_request_counter;
        total_shared_bus_processing_delay/=shared_request_counter;

        total_mem_bus_transfer_queue_delay/=mem_request_counter;
        total_mem_bus_transfer_delay/=mem_request_counter;
        total_mem_bus_processing_queue_delay/=mem_request_counter;
        total_mem_bus_processing_delay/=mem_request_counter;
    }
};
}
#endif