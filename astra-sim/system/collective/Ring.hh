/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __RING_HH__
#define __RING_HH__

#include <assert.h>
#include <math.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <list>
#include <map>
#include <sstream>
#include <tuple>
#include <vector>
#include "Algorithm.hh"
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/MemBus.hh"
#include "astra-sim/system/MyPacket.hh"
#include "astra-sim/system/topology/RingTopology.hh"

namespace AstraSim {
class Ring : public Algorithm {
 public:
  // enum class Type{AllReduce,AllGather,ReduceScatter,AllToAll};
  RingTopology::Direction dimension;
  RingTopology::Direction direction;
  MemBus::Transmition transmition;
  int zero_latency_packets;
  int non_zero_latency_packets;
  int id;
  int current_receiver;
  int current_sender;
  int nodes_in_ring;
  int stream_count;
  int max_count;
  int remained_packets_per_max_count;
  int remained_packets_per_message;
  int parallel_reduce;
  InjectionPolicy injection_policy;
  std::list<MyPacket> packets;
  bool toggle;
  long free_packets;
  long total_packets_sent;
  long total_packets_received;
  uint64_t msg_size;
  std::list<MyPacket*> locked_packets;
  bool processed;
  bool send_back;
  bool NPU_to_MA;

  Ring(
      ComType type,
      int id,
      int layer_num,
      RingTopology* ring_topology,
      uint64_t data_size,
      RingTopology::Direction direction,
      InjectionPolicy injection_policy,
      bool boost_mode);
  virtual void run(EventType event, CallData* data);
  void process_stream_count();
  // void call(EventType event,CallData *data);
  void release_packets();
  virtual void process_max_count();
  void reduce();
  bool iteratable();
  virtual int get_non_zero_latency_packets();
  void insert_packet(Callable* sender);
  bool ready();

  void exit();
};
} // namespace AstraSim
#endif
