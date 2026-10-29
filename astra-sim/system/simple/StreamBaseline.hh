// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <list>

#include <astra-sim/system/simple/BaseStream.hh>
#include <astra-sim/system/simple/CollectivePhase.hh>
#include <astra-sim/system/simple/RecvPacketEventHandlerData.hh>

namespace AstraSim {

class SimpleSys;
class StreamBaseline : public BaseStream {
  public:
    StreamBaseline(SimpleSys* owner,
                   DataSet* dataset,
                   int stream_id,
                   std::list<CollectivePhase> phases_to_go,
                   int priority);

    void init();
    void call(EventType event, CallData* data);
    void consume(RecvPacketEventHandlerData* message);
};

}  // namespace AstraSim
