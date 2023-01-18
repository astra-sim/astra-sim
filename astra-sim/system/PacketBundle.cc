/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/PacketBundle.hh"

using namespace AstraSim;

PacketBundle::PacketBundle(
    Sys* sys,
    BaseStream* stream,
    std::list<MyPacket*> locked_packets,
    bool needs_processing,
    bool send_back,
    int size,
    MemBus::Transmition transmition) {
  this->sys = sys;
  this->locked_packets = locked_packets;
  this->needs_processing = needs_processing;
  this->send_back = send_back;
  this->size = size;
  this->stream = stream;
  this->transmition = transmition;
  creation_time = Sys::boostedTick();
}

PacketBundle::PacketBundle(
    Sys* sys,
    BaseStream* stream,
    bool needs_processing,
    bool send_back,
    int size,
    MemBus::Transmition transmition) {
  this->sys = sys;
  this->needs_processing = needs_processing;
  this->send_back = send_back;
  this->size = size;
  this->stream = stream;
  this->transmition = transmition;
  creation_time = Sys::boostedTick();
}

void PacketBundle::send_to_MA() {
  sys->memBus->send_from_NPU_to_MA(
      transmition, size, needs_processing, send_back, this);
}

void PacketBundle::send_to_NPU() {
  sys->memBus->send_from_MA_to_NPU(
      transmition, size, needs_processing, send_back, this);
}

void PacketBundle::call(EventType event, CallData* data) {
  if (needs_processing == true) {
    needs_processing = false;
    this->delay =
      sys->mem_write(size)
      + sys->mem_read(size)
      + sys->mem_read(size);
    sys->try_register_event(
        this, EventType::CommProcessingFinished, data, this->delay);
    return;
  }
  Tick current = Sys::boostedTick();
  for (auto& packet: locked_packets) {
    packet->ready_time = current;
  }
  stream->call(EventType::General, data);
  delete this;
}
