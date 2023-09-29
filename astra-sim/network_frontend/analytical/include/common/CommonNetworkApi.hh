/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <astra-network-analytical/common/EventQueue.hh>
#include <astra-sim/system/AstraNetworkAPI.hh>
#include <astra-sim/system/Common.hh>
#include "common/CallbackTracker.hh"
#include "common/ChunkIdGenerator.hh"

using namespace AstraSim;
using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;

namespace AstraSimAnalytical {

class CommonNetworkApi : public AstraNetworkAPI {
 public:
  static void set_event_queue(
      std::shared_ptr<EventQueue> event_queue_ptr) noexcept;

  static CallbackTracker& get_callback_tracker() noexcept;

  static void process_chunk_arrival(void* args) noexcept;

  explicit CommonNetworkApi(int rank) noexcept;

  [[nodiscard]] timespec_t sim_get_time() override;

  void sim_schedule(
      timespec_t delta,
      void (*fun_ptr)(void* fun_arg),
      void* fun_arg);

  void schedule(timespec_t time, void (*fun_ptr)(void* fun_arg), void* fun_arg)
      override;

  int sim_recv(
      void* buffer,
      uint64_t count,
      int type,
      int src,
      int tag,
      sim_request* request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg) override;

  double get_BW_at_dimension(int dim) override;

 protected:
  static std::shared_ptr<EventQueue> event_queue;
  static ChunkIdGenerator chunk_id_generator;
  static CallbackTracker callback_tracker;
  static std::vector<Bandwidth> bandwidth_per_dim;
  static int dims_count;
};

} // namespace AstraSimAnalytical
