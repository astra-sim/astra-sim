// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/PacketBundle.hh>

#include <astra-sim/system/simple/SimpleSys.hh>

using namespace AstraSim;

PacketBundle::PacketBundle(SimpleSys* sys,
                           BaseStream* stream,
                           std::list<MyPacket*> locked_packets,
                           bool needs_processing,
                           bool send_back,
                           uint64_t size,
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

PacketBundle::PacketBundle(SimpleSys* sys,
                           BaseStream* stream,
                           bool needs_processing,
                           bool send_back,
                           uint64_t size,
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
    sys->memBus->send_from_NPU_to_MA(transmition, size, needs_processing,
                                     send_back, this);
}

void PacketBundle::send_to_NPU() {
    sys->memBus->send_from_MA_to_NPU(transmition, size, needs_processing,
                                     send_back, this);
}

void PacketBundle::call(EventType event, CallData* data) {
    if (needs_processing == true) {
        needs_processing = false;
        if (sys->local_mem_bw_Bps <= 0) {
            Sys::sysPanic(
                "PacketBundle: local-mem-bw must be configured (>0) in "
                "system-configuration to model local-memory processing "
                "delay.");
        }
        // local_mem_bw_Bps is bytes/sec; register_event expects picoseconds,
        // so scale seconds -> ps with *1e12 (was missing entirely, making
        // this delay silently 0 for any transfer under 1 second).
        this->delay =
            static_cast<uint64_t>(static_cast<double>(size) /
                                  sys->local_mem_bw_Bps * 1e12)  // write
            + static_cast<uint64_t>(static_cast<double>(size) /
                                    sys->local_mem_bw_Bps * 1e12)  // read
            + static_cast<uint64_t>(static_cast<double>(size) /
                                    sys->local_mem_bw_Bps * 1e12);  // read
        sys->register_event(this, EventType::CommProcessingFinished, data,
                            this->delay);
        this->delay = 0;
        return;
    }
    Tick current = Sys::boostedTick();
    for (auto& packet : locked_packets) {
        packet->ready_time = current;
    }
    stream->call(EventType::General, data);
    delete this;
}
