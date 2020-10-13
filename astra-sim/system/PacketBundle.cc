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

#include "PacketBundle.hh"
namespace AstraSim {
PacketBundle::PacketBundle(Sys *generator, BaseStream *stream,
                           std::list<MyPacket *> locked_packets, bool processed,
                           bool send_back, int size,
                           MemBus::Transmition transmition) {
  this->generator = generator;
  this->locked_packets = locked_packets;
  this->processed = processed;
  this->send_back = send_back;
  this->size = size;
  this->stream = stream;
  this->transmition = transmition;
  creation_time = Sys::boostedTick();
}
PacketBundle::PacketBundle(Sys *generator, BaseStream *stream, bool processed,
                           bool send_back, int size,
                           MemBus::Transmition transmition) {
  this->generator = generator;
  this->processed = processed;
  this->send_back = send_back;
  this->size = size;
  this->stream = stream;
  this->transmition = transmition;
  creation_time = Sys::boostedTick();
}
void PacketBundle::send_to_MA() {
  generator->memBus->send_from_NPU_to_MA(transmition, size, processed,
                                         send_back, this);
}
void PacketBundle::send_to_NPU() {
  generator->memBus->send_from_MA_to_NPU(transmition, size, processed,
                                         send_back, this);
}
void PacketBundle::call(EventType event, CallData *data) {
  if (processed == true) {
    processed = false;
    generator->mem_read(size);
    generator->mem_read(size);
    this->delay = generator->mem_write(size);
    generator->try_register_event(this, EventType::CommProcessingFinished, data,
                                  this->delay);
    return;
  }
  Tick current = Sys::boostedTick();
  for (auto &packet : locked_packets) {
    packet->ready_time = current;
  }
  stream->call(EventType::General, data);
  delete this;
}
} // namespace AstraSim