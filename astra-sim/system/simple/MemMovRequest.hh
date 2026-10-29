// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/simple/SharedBusStat.hh>
#include <astra-sim/system/simple/SimpleSys.hh>

namespace AstraSim {

class LogGP;
class MemMovRequest : public Callable, public SharedBusStat {
  public:
    MemMovRequest(int request_num,
                  SimpleSys* sys,
                  LogGP* loggp,
                  int size,
                  int latency,
                  Callable* callable,
                  bool processed,
                  bool send_back);
    void wait_wait_for_mem_bus(std::list<MemMovRequest>::iterator pointer);
    void set_iterator(std::list<MemMovRequest>::iterator pointer) {
        this->pointer = pointer;
    }
    void call(EventType event, CallData* data);

    static int id;
    int my_id;
    int size;
    int latency;
    Callable* callable;
    bool processed;
    bool send_back;
    bool mem_bus_finished;
    SimpleSys* sys;
    EventType callEvent = EventType::General;
    LogGP* loggp;
    std::list<MemMovRequest>::iterator pointer;

    Tick total_transfer_queue_time;
    Tick total_transfer_time;
    Tick total_processing_queue_time;
    Tick total_processing_time;
    Tick start_time;
    int request_num;
};

}  // namespace AstraSim
