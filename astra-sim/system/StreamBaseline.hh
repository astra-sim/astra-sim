/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __STREAM_BASELINE_HH__
#define __STREAM_BASELINE_HH__

#include <list>

#include "astra-sim/system/BaseStream.hh"
#include "astra-sim/system/CollectivePhase.hh"
#include "astra-sim/system/RecvPacketEventHandlerData.hh"

namespace AstraSim {

class Sys;
class StreamBaseline : public BaseStream {
 public:
  StreamBaseline(
      Sys* owner,
      DataSet* dataset,
      int stream_id,
      std::list<CollectivePhase> phases_to_go,
      int priority);

  void init();
  void call(EventType event, CallData* data);
  void consume(RecvPacketEventHandlerData* message);
};

} // namespace AstraSim

#endif /* __STREAM_BASELINE_HH__ */
