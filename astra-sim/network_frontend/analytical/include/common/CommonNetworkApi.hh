// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-network-analytical/common/BaseTopology.h>
#include <astra-network-analytical/common/EventQueue.h>
#include <astra-sim/common/AstraNetworkAPI.hh>
#include <astra-sim/common/Common.hh>
#include <astra-sim/common/SimObject.hh>
#include <astra-sim/common/logging/EventTracer.hh>
#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/common/stats/StatCollection.hh>
#include <astra-sim/common/stats/Statistics.hh>
#include <common/ChunkTracker.hh>
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
class CommonNetworkApi : public AstraNetworkAPI, public SimObject {
  public:
    /**
     * Constructor.
     *
     * @param rank id of the API
     */
    explicit CommonNetworkApi(int rank) noexcept;

    /**
     * Set the event queue to be used.
     *
     * @param event_queue_ptr pointer to the event queue
     */
    static void set_event_queue(
        std::shared_ptr<EventQueue> event_queue_ptr) noexcept;

    /**
     * Get the reference to the chunk tracker.
     *
     * @return reference to the chunk tracker
     */
    static ChunkTracker& get_chunk_tracker() noexcept;

    /**
     * Get static resource_tracker object
     * For use with static func callbacks on events
     *
     * @return (shared) ptr to resource_tracker
     */
    static RequestToResourcesMap& get_resource_tracker() noexcept;

    /**
     * Get static pending_chunks_queue object
     * For use with static func callbacks on events
     *
     * @return (shared) ptr to pending_chunks_queue
     */
    static ResourceToRequestsMap& get_pending_chunks_queue() noexcept;

    /**
     * Get static rejected_requests_queue object
     * For use with static func callbacks on events
     *
     * @return (shared) ptr to rejected_requests_queue
     */
    static ResourceToRequestsMap& get_rejected_requests_queue() noexcept;

    /**
     * Get pointer to topology. Topology is static object shared by all system's
     network APIs
     *
     * @return (shared) pointer to topology
     */
    static std::shared_ptr<BaseTopology> get_topology() noexcept;

    /**
     * Set flag that detailed stats should be used. Initialize detailed stat
     * objects. Begin periodic tracing
     *
     * @param histos_nbins_min_max # bins, min val, and max val for various
     * histograms
     * @param use_detailed_stats whether to use detailed stats
     * @param periodic_stats_period period in ps of periodic stats
     */
    virtual void init_stats(
        const std::vector<std::tuple<int, int, int>>& histos_nbins_min_max,
        bool use_detailed_stats,
        int periodic_stats_period) noexcept;

    /**
     * Override the ctrl messages sizes. Defaults are in constructor
     *
     * @param ack_size size of acks
     * @param cmd_size size of requests for data
     */
    void set_ctrl_sizes(Req_Size ack_size, Req_Size cmd_size) noexcept;

    /**
     * Advance chunk FSM and continue to next steps
     *
     * @param net_req_id request ID of arrived chunk
     */
    static void advance_current_chunk(const RequestID net_req_id) noexcept;

    /**
     * Retry (inside network) pending chunks
     *
     * @param freed_resource recently freed resource to key into chunks to retry
     *
     * @return bool if resource was consumed
     */
    static bool handle_resources_pending_chunks(
        ResourceID freed_resource) noexcept;

    /**
     * Retry (outside network) pending requests
     *
     * @param freed_resource recently freed resource to key into requests to
     * retry
     */
    static void handle_resources_rejected_requests(
        ResourceID freed_resource) noexcept;

    /**
     * Callback to be invoked when a chunk arrives its destination.
     * Implemented/defined by derived Aware/Unaware APIs
     *
     * @param args arguments of the callback function
     */
    static void process_chunk_arrival(void* args) noexcept;

    /**
     * Implement sim_get_time of AstraNetworkAPI.
     */
    [[nodiscard]] timespec_t sim_get_time() const override;

    /**
     * Implement sim_schedule of AstraNetworkAPI.
     */
    void sim_schedule(timespec_t delta,
                      void (*fun_ptr)(void* fun_arg),
                      void* fun_arg) override;

    /**
     * Implement (non-sync) sim_cmd of AstraNetworkAPI.
     */
    int sim_cmd(AstraNetworkAPI::CommandType cmd_type,
                const sim_request& request,
                void (*msg_handler)(void* fun_arg),
                void* fun_arg) override;

    /**
     * Query ChunkTracker for an entry matching the RequestID. Switch-case on
     * the ChunkType and call appropriate function to return constraining
     * resource
     *
     * @param net_req_id the request ID
     *
     * @return the constraining resource. -1 if not blocked and tranmission was
     * successful
     */
    ResourceID handle_network_transception(const RequestID net_req_id);

    /**
     * Try to send the request of a given size between the given network device
     * source and destinations
     *
     * @param net_req_id the request ID
     * @param count the size (in Bytes) of the message
     * @param net_src the network device ID of the source
     * @param net_dst the network device ID of the destination
     *
     * @return the constraining resource. -1 if not blocked and tranmission was
     * successful
     */
    virtual ResourceID try_to_create_and_schedule_chunk(
        const RequestID net_req_id,
        const uint64_t count,
        const int net_src,
        const int net_dst) = 0;

    /**
     * Query topology for the device ID corresponding to the given CU in the
     * given system
     *
     * @param sys_id system ID
     * @param cu_id CU ID (relative to sys_id)
     *
     * @return network device ID
     */
    int calcCUNetID(Sys_ID sys_id, CU_ID cu_id) noexcept;

    /**
     * Query topology for the device ID corresponding to the given buffer in the
     * given system
     *
     * @param sys_id system ID
     * @param buf_off buffer (Byte) offset
     * @param req_size request size (in Bytes)
     *
     * @return network device ID
     */
    int calcBufferNetID(Sys_ID sys_id,
                        Buf_Offset buff_off,
                        uint64_t req_size) noexcept;

    /**
     * Implement get_BW_at_dimension of AstraNetworkAPI.
     */
    double get_BW_at_dimension(int dim) const override;

    /** Override from SimObject */
    const std::string& name() const override;

    /** Override from SimObject */
    Tick curTick() const override;

  protected:
    // for SimObject
    const std::string net_api_name;

    Req_Size ack_ctrl_size;

    Req_Size req_cmd_size;

    /// event queue
    static std::shared_ptr<EventQueue> event_queue;

    // static to be used by static process_chunk_arrival cb
    static std::shared_ptr<BaseTopology> topology;

    // track the resources consumed by a request so that they can be freed on
    // completion
    static RequestToResourcesMap resource_tracker;

    // track the chunks that are waiting on resources to be retried later
    static ResourceToRequestsMap pending_chunks_queue;

    // track (resource limited) request rejections for later callback
    static ResourceToRequestsMap rejected_requests_queue;

    /// chunk tracker
    static ChunkTracker chunk_tracker;

    /// bandwidth per each network dimension of the topology. UNUSED
    static std::vector<Bandwidth> bandwidth_per_dim;

    /// number of network dimensions of the topology
    static int dims_count;

    // hold pointers for the static functions to access member functions
    static std::unordered_map<Sys_ID, CommonNetworkApi*> sys_to_net_ptr_map;

    // track the requests and their managing system
    static std::unordered_map<RequestID, Sys_ID> req_to_sys_map;

    // stats
    bool detailed_stats;

    // stats
    Stat::Counter stat_total_req_ticks;

    // stats
    Stat::Counter stat_total_req_completed;

    // stats
    Stat::Histogram stat_histo_noc_mem_req;
    Stat::Histogram stat_histo_noc_mem_resp;
    Stat::Histogram stat_histo_remote_mem_st;
    Stat::Histogram stat_histo_remote_mem_ack;

    // detailed stats
    Stat::TwoDCounter stat_flow_counter;

    // // detailed stats
    // Stat::TwoDCounter stat_flow_latency;

    // detailed stats
    Stat::TwoDCounter stat_link_usage;
};

}  // namespace AstraSimAnalytical
