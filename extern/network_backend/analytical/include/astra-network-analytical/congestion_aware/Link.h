/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "common/EventQueue.h"
#include "common/Type.h"
#include "congestion_aware/Type.h"
#include <memory>

using namespace NetworkAnalytical;

namespace NetworkAnalyticalCongestionAware {

/**
 * Link models physical links between two devices.
 */
class Link {
  public:
    /**
     * Callback to be called when a link becomes free.
     *  - If the link has pending chunks, process the first one.
     *  - If the link has no pending chunks, set the link as free.
     *
     * @param link_ptr pointer to the link that becomes free
     */
    static void link_become_free(void* link_ptr) noexcept;

    /**
     * Set the event queue to be used by the link.
     *
     * @param event_queue_ptr pointer to the event queue
     */
    static void set_event_queue(std::shared_ptr<EventQueue> event_queue_ptr) noexcept;

    /**
     * Constructor.
     *
     * @param bandwidth bandwidth of the link
     * @param latency latency of the link
     */
    Link(Bandwidth bandwidth, Latency latency) noexcept;

    /**
     * Try to send a chunk through the link.
     * - If the link is free, service the chunk immediately.
     * - If the link is busy, add the chunk to the pending chunks list.
     *
     * @param chunk the chunk to be served by the link
     */
    void send(std::unique_ptr<Chunk> chunk) noexcept;

    /**
     * Dequeue and try to send the first pending chunk
     * in the pending chunks list.
     */
    void process_pending_transmission() noexcept;

    /**
     * Check if the link has pending chunks.
     *
     * @return true if the link has pending chunks, false otherwise
     */
    [[nodiscard]] bool pending_chunk_exists() const noexcept;

    /**
     * Set the link as busy.
     */
    void set_busy() noexcept;

    /**
     * Set the link as free.
     */
    void set_free() noexcept;

  private:
    /// event queue Link uses to schedule events
    static std::shared_ptr<EventQueue> event_queue;

    /// bandwidth of the link in GB/s
    Bandwidth bandwidth;

    /// bandwidth of the link in B/ns, used in actual computation
    Bandwidth bandwidth_Bpns;

    /// latency of the link in ns
    Latency latency;

    /// queue of pending chunks
    std::list<std::unique_ptr<Chunk>> pending_chunks;

    /// flag to indicate if the link is busy
    bool busy;

    /**
     * Compute the serialization delay of a chunk on the link.
     * i.e., serialization delay = (chunk size) / (link bandwidth)
     *
     * @param chunk_size size of the target chunk
     * @return serialization delay of the chunk
     */
    [[nodiscard]] EventTime serialization_delay(ChunkSize chunk_size) const noexcept;

    /**
     * Compute the communication delay of a chunk.
     * i.e., communication delay = (link latency) + (serialization delay)
     *
     * @param chunk_size size of the target chunk
     * @return communication delay of the chunk
     */
    [[nodiscard]] EventTime communication_delay(ChunkSize chunk_size) const noexcept;

    /**
     * Schedule the transmission of a chunk.
     * - Set the link as busy.
     * - Link becomes free after the serialization delay.
     * - Chunk arrives next node after the communication delay.
     *
     * @param chunk chunk to be transmitted
     */
    void schedule_chunk_transmission(std::unique_ptr<Chunk> chunk) noexcept;
};

}  // namespace NetworkAnalyticalCongestionAware
