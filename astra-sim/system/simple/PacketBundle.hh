// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/simple/BaseStream.hh>
#include <astra-sim/system/simple/MemBus.hh>
#include <astra-sim/system/simple/MyPacket.hh>

namespace AstraSim {

class SimpleSys;
class PacketBundle : public Callable {
  public:
    PacketBundle(SimpleSys* sys,
                 BaseStream* stream,
                 std::list<MyPacket*> locked_packets,
                 bool needs_processing,
                 bool send_back,
                 uint64_t size,
                 MemBus::Transmition transmition);
    PacketBundle(SimpleSys* sys,
                 BaseStream* stream,
                 bool needs_processing,
                 bool send_back,
                 uint64_t size,
                 MemBus::Transmition transmition);
    void send_to_MA();
    void send_to_NPU();
    void call(EventType event, CallData* data);

    SimpleSys* sys;
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
