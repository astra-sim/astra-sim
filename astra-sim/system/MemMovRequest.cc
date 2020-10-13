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

#include "MemMovRequest.hh"
#include "LogGP.hh"
#include "Sys.hh"
namespace AstraSim {
int MemMovRequest::id = 0;
MemMovRequest::MemMovRequest(
    int request_num,
    Sys* generator,
    LogGP* loggp,
    int size,
    int latency,
    Callable* callable,
    bool processed,
    bool send_back)
    : SharedBusStat(BusType::Mem, 0, 0, 0, 0) {
  this->size = size;
  this->latency = latency;
  this->callable = callable;
  this->processed = processed;
  this->send_back = send_back;
  this->my_id = id++;
  this->generator = generator;
  this->loggp = loggp;
  this->total_transfer_queue_time = 0;
  this->total_transfer_time = 0;
  this->total_processing_queue_time = 0;
  this->total_processing_time = 0;
  this->request_num = request_num;
  this->start_time = Sys::boostedTick();
  this->mem_bus_finished = true;
}
void MemMovRequest::call(EventType event, CallData* data) {
  update_bus_stats(BusType::Mem, (SharedBusStat*)data);
  total_mem_bus_transfer_delay +=
      ((SharedBusStat*)data)->total_shared_bus_transfer_delay;
  total_mem_bus_processing_delay +=
      ((SharedBusStat*)data)->total_shared_bus_processing_delay;
  total_mem_bus_processing_queue_delay +=
      ((SharedBusStat*)data)->total_shared_bus_processing_queue_delay;
  total_mem_bus_transfer_queue_delay +=
      ((SharedBusStat*)data)->total_shared_bus_transfer_queue_delay;
  mem_request_counter = 1;
  // delete (SharedBusStat *)data;
  // callEvent=EventType::General;
  mem_bus_finished = true;
  loggp->talking_it = pointer;
  loggp->call(callEvent, data);
}
void MemMovRequest::wait_wait_for_mem_bus(
    std::list<MemMovRequest>::iterator pointer) {
  mem_bus_finished = false;
  this->pointer = pointer;
}
} // namespace AstraSim