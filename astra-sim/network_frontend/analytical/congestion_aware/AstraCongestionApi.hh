/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "ChunkIdGenerator.hh"
#include "EventHandlerTracker.hh"
#include "astra-sim/system/AstraMemoryAPI.hh"
#include "astra-sim/system/AstraNetworkAPI.hh"
#include "astra-sim/system/AstraSimDataAPI.hh"
#include "astra-sim/system/Common.hh"
#include "event-queue/EventQueue.hh"
#include "network/Chunk.hh"
#include "network/Link.hh"
#include "topology/Topology.hh"

namespace Congestion {

class AstraCongestionApi : public AstraSim::AstraNetworkAPI {
 public:
  static void link_event_queue(
      std::shared_ptr<EventQueue> event_queue) noexcept;

  static void link_topology(std::shared_ptr<Topology> topology) noexcept;

  static void process_chunk_arrival(void* args) noexcept;

  static EventHandlerTracker& get_event_handler_tracker() noexcept;

  AstraCongestionApi(int rank) noexcept;

  ~AstraCongestionApi() noexcept;

  // int sim_comm_size(AstraSim::sim_comm comm, int* size) override;

  // int sim_finish() override;

  // double sim_time_resolution() override;

  // Divya: to port congestion backend to Chakra
  // int sim_init(AstraSim::AstraMemoryAPI* MEM) override;

  AstraSim::timespec_t sim_get_time() override;
  // Divya: Changes to port congestion backend to Chakra
  void sim_schedule(
      AstraSim::timespec_t delta,
      void (*fun_ptr)(void* fun_arg),
      void* fun_arg);

  void schedule(
      AstraSim::timespec_t delta,
      void (*fun_ptr)(void* fun_arg),
      void* fun_arg) override;

  int sim_send(
      void* buffer,
      uint64_t count,
      int type,
      int dst,
      int tag,
      AstraSim::sim_request* request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg) override;

  int sim_recv(
      void* buffer,
      uint64_t count,
      int type,
      int src,
      int tag,
      AstraSim::sim_request* request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg) override;

  // Divya: to port congestion backend to Chakra
  // void pass_front_end_report(
  //     AstraSim::AstraSimDataAPI astraSimDataAPI) override;

  // double get_BW_at_dimension(int dim) override;

 private:
  static std::shared_ptr<EventQueue> event_queue;
  static std::shared_ptr<Topology> topology;

  static ChunkIdGenerator chunk_id_generator;
  static EventHandlerTracker event_handler_tracker;
};

} // namespace Congestion
