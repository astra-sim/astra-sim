/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __COMMON_HH__
#define __COMMON_HH__

#include <cstdint>

namespace AstraSim {

typedef unsigned long long Tick;

constexpr uint64_t CLOCK_PERIOD = 1;
constexpr uint64_t FREQ = 1275 * 1000 * 1000;

enum time_type_e {
  SE = 0,
  MS,
  US,
  NS,
  FS
};

enum req_type_e {
  UINT8 = 0,
  BFLOAT16,
  FP32
};

struct timespec_t {
  time_type_e time_res;
  double time_val;
};

struct sim_request {
  uint32_t srcRank;
  uint32_t dstRank;
  uint32_t tag;
  req_type_e reqType;
  uint64_t reqCount;
  uint32_t vnet;
  uint32_t layerNum;
};

class MetaData {
 public:
  timespec_t timestamp;
};

enum class ComType {
  None = 0,
  Reduce_Scatter,
  All_Gather,
  All_Reduce,
  All_to_All,
  All_Reduce_All_to_All
};

enum class CollectiveOptimization {
  Baseline = 0,
  LocalBWAware
};

enum class CollectiveImplType {
  Ring = 0,
  OneRing,
  Direct,
  OneDirect,
  AllToAll,
  DoubleBinaryTreeLocalAllToAll,
  LocalRingNodeA2AGlobalDBT,
  HierarchicalRing,
  DoubleBinaryTree,
  HalvingDoubling,
  OneHalvingDoubling,
};

enum class CollectiveBarrier {
  Blocking = 0,
  Non_Blocking
};

enum class SchedulingPolicy {
  LIFO = 0,
  FIFO,
  EXPLICIT,
  None
};

enum class IntraDimensionScheduling {
  FIFO = 0,
  RG,
  SmallestFirst,
  LessRemainingPhaseFirst
};

enum class InterDimensionScheduling {
  Ascending = 0,
  OnlineGreedy,
  RoundRobin,
  OfflineGreedy,
  OfflineGreedyFlex
};

enum class InjectionPolicy {
  Infinite = 0,
  Aggressive,
  SemiAggressive,
  ExtraAggressive,
  Normal
};

enum class PacketRouting {
  Hardware = 0,
  Software
};

enum class BusType {
  Both = 0,
  Shared,
  Mem
};

enum class StreamState {
  Created = 0,
  Transferring,
  Ready,
  Executing,
  Zombie,
  Dead
};

enum class EventType {
  CallEvents = 0,
  General,
  RendezvousSend,
  RendezvousRecv,
  PacketReceived,
  PacketSent,
  Rec_Finished,
  Send_Finished,
  Processing_Finished,
  NPU_to_MA,
  MA_to_NPU,
  Consider_Process,
  Consider_Retire,
  Consider_Send_Back,
  StreamInit,
  CommProcessingFinished,
  CollectiveCommunicationFinished,
  CompFinished,
  MemLoadFinished,
  MemStoreFinished
};

class CloneInterface {
 public:
  virtual CloneInterface* clone() const = 0;
  virtual ~CloneInterface() = default;
};

class CollectiveImpl : public CloneInterface {
 public:
  CollectiveImpl(CollectiveImplType type) {
    this->type = type;
  };
  virtual CloneInterface* clone() const {
    return new CollectiveImpl(*this);
  }

  CollectiveImplType type;
};

class DirectCollectiveImpl : public CollectiveImpl {
 public:
  CloneInterface* clone() const {
    return new DirectCollectiveImpl(*this);
  };
  DirectCollectiveImpl(
      CollectiveImplType type,
      int direct_collective_window)
      : CollectiveImpl(type) {
    this->direct_collective_window = direct_collective_window;
  }

  int direct_collective_window;
};

} // namespace AstraSim

#endif /* __COMMON_HH__ */
