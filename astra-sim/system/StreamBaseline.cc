/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "StreamBaseline.hh"
#include "astra-sim/system/collective/Algorithm.hh"
namespace AstraSim {
StreamBaseline::StreamBaseline(
    Sys* owner,
    DataSet* dataset,
    int stream_num,
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
  last_init = Sys::boostedTick();
  if (!my_current_phase.enabled) {
    return;
  }
  my_current_phase.algorithm->run(EventType::StreamInit, nullptr);
  if (steps_finished == 1) {
    queuing_delay.push_back(last_phase_change - creation_time);
  }
  queuing_delay.push_back(Sys::boostedTick() - last_phase_change);
  total_packets_sent = 1;
}
void StreamBaseline::call(EventType event, CallData* data) {
  if (event == EventType::WaitForVnetTurn) {
    owner->proceed_to_next_vnet_baseline(this);
    return;
  } else {
    // std::cout<<"general event called in stream"<<std::endl;
    SharedBusStat* sharedBusStat = (SharedBusStat*)data;
    update_bus_stats(BusType::Both, sharedBusStat);
    my_current_phase.algorithm->run(EventType::General, data);
    if (data != nullptr) {
      delete sharedBusStat;
    }
  }
}
void StreamBaseline::consume(RecvPacketEventHadndlerData* message) {
  net_message_latency.back() +=
      Sys::boostedTick() - message->ready_time; // not accurate
  net_message_counter++;
  /*if (owner->id == 0) {
    std::cout<<"message received at time: "
    <<owner->boostedTick()<<std::endl;
  }*/
  my_current_phase.algorithm->run(EventType::PacketReceived, message);
}
} // namespace AstraSim