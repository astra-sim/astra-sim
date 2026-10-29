// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/CallData.hh>
#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/system/simple/StreamStat.hh>

namespace AstraSim {

class DataSet : public Callable, public StreamStat {
  public:
    DataSet(int total_streams);
    void set_notifier(Callable* layer, EventType event);
    void notify_stream_finished(StreamStat* data);
    void call(EventType event, CallData* data);
    bool is_finished();

    static int id_auto_increment;
    int my_id;
    int total_streams;
    int finished_streams;
    bool finished;
    bool active;
    Tick finish_tick;
    Tick creation_tick;
    std::pair<Callable*, EventType>* notifier;
};

}  // namespace AstraSim
