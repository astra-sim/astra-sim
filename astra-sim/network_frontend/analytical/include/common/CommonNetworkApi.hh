/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "common/CallbackTracker.hh"
#include "common/ChunkIdGenerator.hh"
#include <astra-network-analytical/common/EventQueue.h>
#include <astra-sim/common/AstraNetworkAPI.hh>
#include <astra-sim/common/Common.hh>
#include <memory>
#include <vector>

using namespace AstraSim;
using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;

namespace AstraSimAnalytical {

/**
 * CommonNetworkApi implements common AstraNetworkAPI interface
 * that both congestion_unaware and congestion_aware network API inherit.
 */
class CommonNetworkApi : public AstraNetworkAPI {
  public:
    /**
     * Set the event queue to be used.
     *
     * @param event_queue_ptr pointer to the event queue
     */
    static void set_event_queue(
        std::shared_ptr<EventQueue> event_queue_ptr) noexcept;

    /**
     * Get the reference to the callback tracker.
     *
     * @return reference to the callback tracker
     */
    static CallbackTracker& get_callback_tracker() noexcept;

    /**
     * Callback to be invoked when a chunk arrives its destination.
     *
     * @param args arguments of the callback function
     */
    static void process_chunk_arrival(void* args) noexcept;

    /**
     * Constructor.
     *
     * @param rank id of the API
     */
    explicit CommonNetworkApi(int rank) noexcept;

    /**
     * Implement sim_get_time of AstraNetworkAPI.
     */
    [[nodiscard]] timespec_t sim_get_time() override;

    /**
     * Implement sim_schedule of AstraNetworkAPI.
     */
    void sim_schedule(timespec_t delta,
                      void (*fun_ptr)(void* fun_arg),
                      void* fun_arg) override;

    /**
     * Implement sim_recv of AstraNetworkAPI.
     */
    int sim_recv(void* buffer,
                 uint64_t count,
                 int type,
                 int src,
                 int tag,
                 sim_request* request,
                 void (*msg_handler)(void* fun_arg),
                 void* fun_arg) override;

    /**
     * Implement get_BW_at_dimension of AstraNetworkAPI.
     */
    double get_BW_at_dimension(int dim) override;

  protected:
    /// event queue
    static std::shared_ptr<EventQueue> event_queue;

    /// chunk id generator
    static ChunkIdGenerator chunk_id_generator;

    /// callback tracker
    static CallbackTracker callback_tracker;

    /// bandwidth per each network dimension of the topology
    static std::vector<Bandwidth> bandwidth_per_dim;

    /// number of network dimensions of the topology
    static int dims_count;
};

}  // namespace AstraSimAnalytical
