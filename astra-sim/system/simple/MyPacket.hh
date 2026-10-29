// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/Callable.hh>

namespace AstraSim {

class MyPacket : public Callable {
  public:
    MyPacket(int preferred_vnet, int preferred_src, int preferred_dest);
    MyPacket(uint64_t msg_size,
             int preferred_vnet,
             int preferred_src,
             int preferred_dest);
    void set_notifier(Callable* c);
    void call(EventType event, CallData* data);

    int cycles_needed;
    int fm_id;
    int stream_id;
    Callable* notifier;
    Callable* sender;
    int preferred_vnet;
    int preferred_dest;
    int preferred_src;
    uint64_t msg_size;
    Tick ready_time;
};

}  // namespace AstraSim
