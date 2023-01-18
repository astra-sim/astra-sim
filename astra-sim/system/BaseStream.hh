/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __BASE_STREAM_HH__
#define __BASE_STREAM_HH__

#include <map>
#include <list>

#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/CollectivePhase.hh"
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/DataSet.hh"
#include "astra-sim/system/StreamStat.hh"
#include "astra-sim/system/Sys.hh"
#include "astra-sim/system/topology/LogicalTopology.hh"

namespace AstraSim {

class RecvPacketEventHandlerData;
class BaseStream : public Callable, public StreamStat {
 public:
  BaseStream(
      int stream_id,
      Sys* owner,
      std::list<CollectivePhase> phases_to_go);
  virtual ~BaseStream() = default;

  void changeState(StreamState state);
  virtual void consume(RecvPacketEventHandlerData* message) = 0;
  virtual void init() = 0;

  static std::map<int, int> synchronizer;
  static std::map<int, int> ready_counter;
  static std::map<int, std::list<BaseStream*> > suspended_streams;
  int stream_id;
  int total_packets_sent;
  SchedulingPolicy preferred_scheduling;
  std::list<CollectivePhase> phases_to_go;
  int current_queue_id;
  CollectivePhase my_current_phase;
  ComType current_com_type;
  Tick creation_time;
  Tick last_init;
  Sys* owner;
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

} // namespace AstraSim

#endif /* __BASE_STREAM_HH__ */
