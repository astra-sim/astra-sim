// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <list>

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/simple/MemBus.hh>
#include <astra-sim/system/simple/MemMovRequest.hh>

namespace AstraSim {

class SimpleSys;
class LogGP : public Callable {
  public:
    LogGP(std::string name,
          SimpleSys* sys,
          Tick L,
          Tick o,
          Tick g,
          double G,
          EventType trigger_event);
    ~LogGP();
    void process_next_read();
    void request_read(int bytes,
                      bool processed,
                      bool send_back,
                      Callable* callable);
    void switch_to_receiver(MemMovRequest mr, Tick offset);
    void call(EventType event, CallData* data);
    void attach_mem_bus(SimpleSys* sys,
                        Tick L,
                        Tick o,
                        Tick g,
                        double G,
                        bool model_shared_bus,
                        int communication_delay);

    enum class State { Free, waiting, Sending, Receiving };
    enum class ProcState { Free, Processing };

    MemBus* NPU_MEM;
    int request_num;
    std::string name;
    Tick L;
    Tick o;
    Tick g;
    double G;
    Tick last_trans;
    State curState;
    State prevState;
    ProcState processing_state;
    std::list<MemMovRequest> sends;
    std::list<MemMovRequest> receives;
    std::list<MemMovRequest> processing;

    std::list<MemMovRequest> retirements;
    std::list<MemMovRequest> pre_send;
    std::list<MemMovRequest> pre_process;
    std::list<MemMovRequest>::iterator talking_it;

    LogGP* partner;
    SimpleSys* sys;
    EventType trigger_event;
    int subsequent_reads;
    int THRESHOLD;
    int local_reduction_delay;
};

}  // namespace AstraSim
