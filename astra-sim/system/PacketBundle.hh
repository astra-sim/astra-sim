/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __PACKET_BUNDLE_HH__
#define __PACKET_BUNDLE_HH__

#include "astra-sim/system/BaseStream.hh"
#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/MemBus.hh"
#include "astra-sim/system/MyPacket.hh"

namespace AstraSim {

class Sys;
class PacketBundle : public Callable {
 public:
  PacketBundle(
      Sys* sys,
      BaseStream* stream,
      std::list<MyPacket*> locked_packets,
      bool needs_processing,
      bool send_back,
      int size,
      MemBus::Transmition transmition);
  PacketBundle(
      Sys* sys,
      BaseStream* stream,
      bool needs_processing,
      bool send_back,
      int size,
      MemBus::Transmition transmition);
  void send_to_MA();
  void send_to_NPU();
  void call(EventType event, CallData* data);

  Sys* sys;
  std::list<MyPacket*> locked_packets;
  bool needs_processing;
  bool send_back;
  int size;
  BaseStream* stream;
  MemBus::Transmition transmition;
  Tick delay;
  Tick creation_time;
};

} // namespace AstraSim

#endif /* __PACKET_BUNDLE_HH__ */
