/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <astra-sim/system/AstraNetworkAPI.hh>
#include <astra-sim/system/Common.hh>
#include <network_backend/analytical/common/Common.hh>
#include <network_backend/analytical/common/event-queue/EventQueue.hh>
#include <network_backend/analytical/congestion_aware/topology/Topology.hh>
#include "network_frontend/analytical/congestion_aware/ChunkIdGenerator.hh"
#include "network_frontend/analytical/congestion_aware/EventHandlerTracker.hh"

using namespace AstraSim;
using namespace NetworkAnalyticalCongestionAware;

namespace AstraSimAnalyticalCongestionAware {

class NetworkApi : public AstraNetworkAPI {
 public:
  static void set_event_queue(
      std::shared_ptr<EventQueue> event_queue_ptr) noexcept;

  static void set_topology(std::shared_ptr<Topology> topology_ptr) noexcept;

  static void process_chunk_arrival(void* args) noexcept;

  static EventHandlerTracker& get_event_handler_tracker() noexcept;

  explicit NetworkApi(int rank) noexcept;

  [[nodiscard]] timespec_t sim_get_time() override;
  void sim_schedule(
      timespec_t delta,
      void (*fun_ptr)(void* fun_arg),
      void* fun_arg);

  void schedule(timespec_t time, void (*fun_ptr)(void* fun_arg), void* fun_arg)
      override;

  int sim_send(
      void* buffer,
      uint64_t count,
      int type,
      int dst,
      int tag,
      sim_request* request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg) override;

  int sim_recv(
      void* buffer,
      uint64_t count,
      int type,
      int src,
      int tag,
      sim_request* request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg) override;

 private:
  static std::shared_ptr<EventQueue> event_queue;
  static std::shared_ptr<Topology> topology;
  static ChunkIdGenerator chunk_id_generator;
  static EventHandlerTracker event_handler_tracker;
};

} // namespace AstraSimAnalyticalCongestionAware
