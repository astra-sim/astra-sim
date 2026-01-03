/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __PACKET_BUNDLE_HH__
#define __PACKET_BUNDLE_HH__

#include "astra-sim/common/Common.hh"
#include "astra-sim/system/gpu_system/gpu_model/BaseStream.hh"
#include "astra-sim/system/gpu_system/gpu_model/Callable.hh"
#include "astra-sim/system/gpu_system/gpu_model/MemBus.hh"
#include "astra-sim/system/gpu_system/gpu_model/MyPacket.hh"

namespace AstraSim {

class Sys;
class PacketBundle : public Callable {
  public:
    PacketBundle(Sys* sys,
                 BaseStream* stream,
                 std::list<MyPacket*> locked_packets,
                 bool needs_processing,
                 bool send_back,
                 uint64_t size,
                 MemBus::Transmition transmition);
    PacketBundle(Sys* sys,
                 BaseStream* stream,
                 bool needs_processing,
                 bool send_back,
                 uint64_t size,
                 MemBus::Transmition transmition);
    void send_to_MA();
    void send_to_NPU();
    void call(EventType event, CallData* data);

    Sys* sys;
    std::list<MyPacket*> locked_packets;
    bool needs_processing;
    bool send_back;
    uint64_t size;
    BaseStream* stream;
    MemBus::Transmition transmition;
    Tick delay;
    Tick creation_time;
};

}  // namespace AstraSim

#endif /* __PACKET_BUNDLE_HH__ */
