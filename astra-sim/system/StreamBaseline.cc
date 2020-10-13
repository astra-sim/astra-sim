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

#include "StreamBaseline.hh"
#include "astra-sim/system/collective/Algorithm.hh"
namespace AstraSim {
StreamBaseline::StreamBaseline(Sys *owner, DataSet *dataset, int stream_num,
                               std::list<CollectivePhase> phases_to_go,
                               int priority)
    : BaseStream(stream_num, owner, phases_to_go) {
  this->owner = owner;
  this->stream_num = stream_num;
  this->phases_to_go = phases_to_go;
  this->dataset = dataset;
  this->priority = priority;
  steps_finished = 0;
  initial_data_size = phases_to_go.front().initial_data_size;
}
void StreamBaseline::init() {
  // std::cout<<"stream number: "<<stream_num<<"is inited in node:
  // "<<owner->id<<std::endl;
  initialized = true;
  if (!my_current_phase.enabled) {
    return;
  }
  my_current_phase.algorithm->run(EventType::StreamInit, NULL);
  if (steps_finished == 1) {
    queuing_delay.push_back(last_phase_change - creation_time);
  }
  queuing_delay.push_back(Sys::boostedTick() - last_phase_change);
  total_packets_sent = 1;
}
void StreamBaseline::call(EventType event, CallData *data) {
  if (event == EventType::WaitForVnetTurn) {
    owner->proceed_to_next_vnet_baseline(this);
    return;
  } else {
    // std::cout<<"general event called in stream"<<std::endl;
    SharedBusStat *sharedBusStat = (SharedBusStat *)data;
    update_bus_stats(BusType::Both, sharedBusStat);
    my_current_phase.algorithm->run(EventType::General, data);
    if (data != NULL) {
      delete sharedBusStat;
    }
  }
}
void StreamBaseline::consume(RecvPacketEventHadndlerData *message) {
  if (message->message_end) {
    net_message_latency.back() +=
        Sys::boostedTick() - message->ready_time; // not accurate
    net_message_counter++;
    if (steps_finished == 1 && owner->id == 0) {
      // std::cout<<"receiving finshed message flag for stream:
      // "<<stream_num<<std::endl;
    }
  }
  my_current_phase.algorithm->run(EventType::PacketReceived, message);
}
} // namespace AstraSim