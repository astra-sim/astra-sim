/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ANALYTICAL_NETWORK_HH__
#define __ANALYTICAL_NETWORK_HH__

#include <memory>

#include "PayloadSizeTracker.hh"
#include "SendRecvTrackingMap.hh"
#include "astra-sim/system/AstraNetworkAPI.hh"
#include "event-queue/EventQueue.hh"
#include "topology/CostModel.hh"
#include "topology/Topology.hh"

namespace Analytical {
class AnalyticalNetwork : public AstraSim::AstraNetworkAPI {
 public:
  AnalyticalNetwork(int rank, int dims_count) noexcept;

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

  void sim_schedule(
      AstraSim::timespec_t delta,
      void (*fun_ptr)(void* fun_arg),
      void* fun_arg);

  void schedule(
      AstraSim::timespec_t event_time,
      void (*fun_ptr)(void* fun_arg),
      void* fun_arg) override;

  AstraSim::timespec_t sim_get_time() override;

  double get_BW_at_dimension(int dim) override;

  // AnalyticalNetwork-specific functions
  static void set_event_queue(
      const std::shared_ptr<EventQueue>& event_queue_ptr) noexcept;

  static void set_topology(
      const std::shared_ptr<Topology>& topology_ptr) noexcept;

  static void set_cost_model(CostModel* const cost_model_ptr) noexcept;

 private:
  static std::shared_ptr<EventQueue> event_queue;
  static std::shared_ptr<Topology> topology;
  static SendRecvTrackingMap send_recv_tracking_map;
  static std::shared_ptr<PayloadSizeTracker> payload_size_tracker;
  static CostModel* cost_model;

  int dims_count;
};
} // namespace Analytical

#endif /* __ANALYTICAL_NETWORK_HH__ */
