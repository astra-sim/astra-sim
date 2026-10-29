// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <list>
#include <map>

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/simple/CollectivePhase.hh>
#include <astra-sim/system/simple/DataSet.hh>
#include <astra-sim/system/simple/StreamStat.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/LogicalTopology.hh>

namespace AstraSim {

class SimpleSys;
class RecvPacketEventHandlerData;
class BaseStream : public Callable, public StreamStat {
  public:
    BaseStream(int stream_id,
               SimpleSys* owner,
               std::list<CollectivePhase> phases_to_go);
    virtual ~BaseStream() = default;

    void changeState(StreamState state);
    virtual void consume(RecvPacketEventHandlerData* message) = 0;
    virtual void init() = 0;

    static std::map<int, int> synchronizer;
    static std::map<int, int> ready_counter;
    static std::map<int, std::list<BaseStream*>> suspended_streams;
    int stream_id;
    int total_packets_sent;
    SchedulingPolicy preferred_scheduling;
    std::list<CollectivePhase> phases_to_go;
    int current_queue_id;
    CollectivePhase my_current_phase;
    ComType current_com_type;
    Tick creation_time;
    Tick last_init;
    SimpleSys* owner;
    DataSet* dataset;
    int steps_finished;
    int initial_data_size;
    int priority;
    StreamState state;
    bool initialized;

    Tick last_phase_change;

    int test;
    int test2;
    uint64_t phase_latencies[10];
};

}  // namespace AstraSim
