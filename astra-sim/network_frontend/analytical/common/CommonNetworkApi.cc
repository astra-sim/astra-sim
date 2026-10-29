// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>
#include <common/CommonNetworkApi.hh>

// for prints
#include <iomanip>
#include <iostream>

#define ACCEPT_ALL_REQUESTS 1

using namespace AstraSim;
using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;

std::shared_ptr<EventQueue> CommonNetworkApi::event_queue = nullptr;

std::shared_ptr<BaseTopology> CommonNetworkApi::topology;

ChunkTracker CommonNetworkApi::chunk_tracker = {};

RequestToResourcesMap CommonNetworkApi::resource_tracker = {};

ResourceToRequestsMap CommonNetworkApi::pending_chunks_queue = {};

ResourceToRequestsMap CommonNetworkApi::rejected_requests_queue = {};

int CommonNetworkApi::dims_count = -1;

std::vector<Bandwidth> CommonNetworkApi::bandwidth_per_dim = {};

std::unordered_map<Sys_ID, CommonNetworkApi*>
    CommonNetworkApi::sys_to_net_ptr_map = {};

CommonNetworkApi::CommonNetworkApi(const int rank) noexcept
    : AstraNetworkAPI(rank),
      net_api_name("NetworkApi_" + std::to_string(rank)),
      detailed_stats(false),
      ack_ctrl_size(8),
      req_cmd_size(16) {
    assert(rank >= 0);
    CommonNetworkApi::sys_to_net_ptr_map[(Sys_ID)rank] = this;
}

void CommonNetworkApi::set_event_queue(
    std::shared_ptr<EventQueue> event_queue_ptr) noexcept {
    assert(event_queue_ptr != nullptr);

    CommonNetworkApi::event_queue = std::move(event_queue_ptr);
}

ChunkTracker& CommonNetworkApi::get_chunk_tracker() noexcept {
    return chunk_tracker;
}

RequestToResourcesMap& CommonNetworkApi::get_resource_tracker() noexcept {
    return resource_tracker;
}

ResourceToRequestsMap& CommonNetworkApi::get_pending_chunks_queue() noexcept {
    return pending_chunks_queue;
}

ResourceToRequestsMap& CommonNetworkApi::
    get_rejected_requests_queue() noexcept {
    return rejected_requests_queue;
}

std::shared_ptr<BaseTopology> CommonNetworkApi::get_topology() noexcept {
    return topology;
}

void CommonNetworkApi::set_ctrl_sizes(Req_Size ack_size,
                                      Req_Size cmd_size) noexcept {
    ack_ctrl_size = ack_size;
    req_cmd_size = cmd_size;
}

void CommonNetworkApi::init_stats(
    const std::vector<std::tuple<int, int, int>>& histos_nbins_min_max,
    bool use_detailed_stats,
    int periodic_stats_period) noexcept {
    assert(CommonNetworkApi::topology != nullptr);

    // simple counter stats
    stat_total_req_ticks.init(this, "total_req_ticks",
                              "Total # ticks for requests for this sys");
    stat_total_req_completed.init(this, "total_req_completed",
                                  "Total # requests for this sys");

    // histogram stats
    // TODO figure best way to pass these params? This is ugly but 12 ints need
    // to be given somehow
    assert(histos_nbins_min_max.size() == 4);
    auto tmp_tuple = histos_nbins_min_max[0];
    stat_histo_noc_mem_req.init(
        this, "histo_noc_mem_req_ticks",
        "Histogram of CU->mem cmd request ticks for this sys",
        std::get<0>(tmp_tuple), std::get<1>(tmp_tuple), std::get<2>(tmp_tuple));
    tmp_tuple = histos_nbins_min_max[1];
    stat_histo_noc_mem_resp.init(
        this, "histo_noc_mem_resp_ticks",
        "Histogram of (local) mem->CU data ticks for this sys",
        std::get<0>(tmp_tuple), std::get<1>(tmp_tuple), std::get<2>(tmp_tuple));
    tmp_tuple = histos_nbins_min_max[2];
    stat_histo_remote_mem_st.init(
        this, "histo_remote_mem_st_ticks",
        "Histogram of CU-> (remote) mem ticks for this sys",
        std::get<0>(tmp_tuple), std::get<1>(tmp_tuple), std::get<2>(tmp_tuple));
    tmp_tuple = histos_nbins_min_max[3];
    stat_histo_remote_mem_ack.init(
        this, "histo_remote_mem_ack_ticks",
        "Histogram of (remote) mem->CU ack ticks for this sys",
        std::get<0>(tmp_tuple), std::get<1>(tmp_tuple), std::get<2>(tmp_tuple));

    detailed_stats = use_detailed_stats;
    if (!use_detailed_stats) {
        return;
    }
    // detailed below

    stat_flow_counter.init(this, "flow_counter",
                           "# packets/messages per flow (src, dest)");
    // stat_flow_latency.init(this,"flow_latency_counter","Cumulative ticks per
    // flow (src, dest)");
    stat_link_usage.init(this, "link_usage",
                         "# packets/messages over a link (i,j)");
}

timespec_t CommonNetworkApi::sim_get_time() const {
    // get current time from event queue
    const auto current_time = event_queue->get_current_time();

    // return the current time in ASTRA-sim format
    const auto astra_sim_time = static_cast<Tick>(current_time);
    return {PS, astra_sim_time};
}

void CommonNetworkApi::sim_schedule(const timespec_t delta,
                                    void (*fun_ptr)(void*),
                                    void* const fun_arg) {
    assert(delta.time_res == PS);
    assert(fun_ptr != nullptr);

    // calculate absolute event time
    const auto current_time = sim_get_time();
    const auto event_time = current_time.time_val + delta.time_val;
    const auto event_time_ps = static_cast<EventTime>(event_time);

    // schedule the event to the event queue
    assert(event_time_ps >= event_queue->get_current_time());
    event_queue->schedule_event(event_time_ps, fun_ptr, fun_arg);
}

int CommonNetworkApi::sim_cmd(CommandType cmd_type,
                              const sim_request& request,
                              void (*msg_handler)(void*),
                              void* const fun_arg) {

    /**
     * 1) Decide values for ChunkTrackerEntry
     * 2) Register callback
     * 3) Create ChunkTrackerEntry
     * 4) Kick off network transmission
     * 5) On rejection/failure clean up
     */

    // 1)
    // take the requests ID
    const RequestID net_req_id = request.astraReqId;

    if (cmd_type == CommandType::Recv) {
        // Recv never models its own network transfer -- it either finds the
        // matching Send already Completed and fires right away, or marks
        // itself waiting for that Send's completion to fire it later.
        auto* const entry = chunk_tracker.create_new_entry(cmd_type, request);
        const bool send_already_done =
            entry->get_current_state() == AllChunkStates::Completed;
        DPRINTF(
            "[sim_cmd] Recv request ID %d %u -> %u (%d B): %s\n", net_req_id,
            request.srcRank, request.dstRank, request.reqSizeNBytes,
            send_already_done ? "send done, firing now" : "waiting for send");
        if (send_already_done) {
            const auto immediate_delta = timespec_t({PS, 0});
            sim_schedule(immediate_delta, msg_handler, fun_arg);
            chunk_tracker.pop_entry(net_req_id);
        } else {
            register_callback(net_req_id, CallbackFunc(msg_handler, fun_arg));
            static_cast<SendRecvChunkTrackerEntry*>(entry)->mark_recv_posted();
        }
        return true;
    }

    // 2) set up callback
    register_callback(net_req_id, CallbackFunc(msg_handler, fun_arg));

    // 3) set up entry and some FE tracking
    auto* const new_entry = chunk_tracker.create_new_entry(cmd_type, request);
    new_entry->set_send_request_from_sim_request(&request,
                                                 (Sys_ID)sim_comm_get_rank());
    if (cmd_type == CommandType::Send) {
        // Unstarted -> SrcToDstGPU_Data: only the Send side has a real
        // transfer to run.
        new_entry->transition_next_state();
    }

    DPRINTF("[sim_cmd] Handling %s request ID %d from %u -> %u\n",
            getCmdTypeName(cmd_type), net_req_id, (Sys_ID)sim_comm_get_rank(),
            request.node);

    // 4) kick off transmission
    // can access chunk_tracker for sim_request / status information
    int constraining_resource = handle_network_transception(net_req_id);

    // 5) clean up on failure
    bool tx_success = true;
    if (constraining_resource != -1) {
        // failure -> will retry callback later

        // clang-format off

    #ifdef ACCEPT_ALL_REQUESTS
        // tx_success is true (keep request in network)
        pending_chunks_queue[constraining_resource].push(net_req_id);
    #else
        tx_success = false;
        // post to rejected_requests_queue for later retry
        rejected_requests_queue[constraining_resource].push(net_req_id);
    #endif

        // clang-format on
    }

    // on success let caller handle matching (if needed)
    return tx_success;
}

ResourceID CommonNetworkApi::handle_network_transception(
    const RequestID net_req_id) {
    // base_entry is a std::optional<ChunkTrackerEntry*>
    auto optional_entry = chunk_tracker.search_entry(net_req_id);
    assert(optional_entry.has_value());  // entry must exist

    // work with ChunkTrackerEntry. all needed functions are in base class
    auto entry = *optional_entry;

    AllChunkStates chunk_status = entry->get_current_state();
    const auto tx_request = entry->get_send_request();
    assert(tx_request);  // assert request was set previously

    uint64_t count = -1;
    int net_src = -1;
    int net_dst = -1;

    // if statements for better formatting
    if (chunk_status == AllChunkStates::Completed) {

        // a) complete and call cb
        DPRINTF("Retrieving cb for %d\n", net_req_id);

        // will be this sys
        auto chunk_cb = retrieve_callback(net_req_id);
        const auto immediate_delta = timespec_t({PS, 0});
        sim_schedule(immediate_delta, chunk_cb.first, chunk_cb.second);

        DPRINTF("Completed chunk. Calling callback%s\n", "");

        // A Send's own callback (above) fires unconditionally. If the Recv
        // already posted, fire it too and fall through to pop_entry(). If
        // not, leave the entry at Completed (do not pop) for the Recv's own
        // sim_cmd() to find and fire later.
        if (entry->get_chunk_type() == AstraNetworkAPI::CommandType::Send) {
            auto* const sr_entry =
                static_cast<SendRecvChunkTrackerEntry*>(entry);
            if (!sr_entry->is_recv_posted()) {
                DPRINTF("Send request ID %d done; no recv posted, parking "
                        "entry\n",
                        net_req_id);
                return -1;
            }
            DPRINTF("Send request ID %d done; notifying recv on rank %u\n",
                    net_req_id, tx_request->dstRank);
            auto* const recv_net_ptr =
                sys_to_net_ptr_map.at(tx_request->dstRank);
            const auto recv_cb = recv_net_ptr->retrieve_callback(net_req_id);
            recv_net_ptr->sim_schedule(immediate_delta, recv_cb.first,
                                       recv_cb.second);
        }

        // b) remove
        chunk_tracker.pop_entry(net_req_id);

        // assert everything was cleaned
        assert(!chunk_tracker.search_entry(net_req_id).has_value());
        assert(resource_tracker.count(net_req_id) == 0);

        // return success (-1) early
        return -1;

    } else if (chunk_status == AllChunkStates::DstBufToSrcCU_Data) {

        // parse out relevant tx information
        Sys_ID rx_sys = tx_request->srcRank;
        Sys_ID tx_sys = tx_request->dstRank;
        Buf_Offset tx_buf_off = tx_request->bufByteOff;
        CU_ID rx_cu = tx_request->reqCuId;

        count = tx_request->reqSizeNBytes;
        net_dst = calcCUNetID(rx_sys, rx_cu);
        net_src = calcBufferNetID(tx_sys, tx_buf_off, count);

        DPRINTF("Handling chunk of status DstBufToSrcCU_Data %s\n", "");

    } else if (chunk_status == AllChunkStates::DstBufToSrcCU_AckCtrl) {
        // Remote store data reached the destination HBM and now sending ack
        // release semaphore here
        const auto vaddr =
            static_cast<RemoteStoreChunkTrackerEntry*>(entry)->vaddr();
        release_semaphore(vaddr);

        // parse out relevant tx information
        Sys_ID rx_sys = tx_request->srcRank;
        Sys_ID tx_sys = tx_request->dstRank;
        Buf_Offset tx_buf_off = tx_request->bufByteOff;
        CU_ID rx_cu = tx_request->reqCuId;

        count = ack_ctrl_size;
        net_dst = calcCUNetID(rx_sys, rx_cu);
        net_src = calcBufferNetID(tx_sys, tx_buf_off, count);

        DPRINTF("Handling chunk of status DstBufToSrcCU_AckCtrl %s\n", "");

    } else if (chunk_status == AllChunkStates::SrcCUToDstBuf_ReqCmd) {

        // parse out relevant tx information
        Sys_ID tx_sys = tx_request->srcRank;
        Sys_ID rx_sys = tx_request->dstRank;
        Buf_Offset rx_buf_off = tx_request->bufByteOff;
        CU_ID tx_cu = tx_request->reqCuId;

        count = req_cmd_size;
        net_src = calcCUNetID(tx_sys, tx_cu);
        net_dst = calcBufferNetID(rx_sys, rx_buf_off, count);

        DPRINTF("Handling chunk of status SrcCUToDstBuf_ReqCmd %s\n", "");

    } else if (chunk_status == AllChunkStates::SrcCUToDstBuf_Data) {

        // parse out relevant tx information
        Sys_ID tx_sys = tx_request->srcRank;
        Sys_ID rx_sys = tx_request->dstRank;
        Buf_Offset rx_buf_off = tx_request->bufByteOff;
        CU_ID tx_cu = tx_request->reqCuId;

        count = tx_request->reqSizeNBytes;
        net_src = calcCUNetID(tx_sys, tx_cu);
        net_dst = calcBufferNetID(rx_sys, rx_buf_off, count);

        DPRINTF("Handling chunk of status SrcCUToDstBuf_Data %s\n", "");

    } else if (chunk_status == AllChunkStates::SrcBufToSrcCU_AckCtrl) {
        // Local store data reached the destination HBM and now sending ack
        // release semaphore here
        const auto vaddr =
            static_cast<LocalStoreChunkTrackerEntry*>(entry)->vaddr();
        release_semaphore(vaddr);

        Sys_ID sys = tx_request->srcRank;
        Buf_Offset buf_off = tx_request->bufByteOff;
        CU_ID cu = tx_request->reqCuId;

        count = ack_ctrl_size;
        net_src = calcBufferNetID(sys, buf_off, count);
        net_dst = calcCUNetID(sys, cu);

        DPRINTF("Handling chunk of status SrcBufToSrcCU_AckCtrl %s\n", "");

    } else if (chunk_status == AllChunkStates::SrcBufToSrcCU_Data) {

        Sys_ID sys = tx_request->srcRank;
        Buf_Offset buf_off = tx_request->bufByteOff;
        CU_ID cu = tx_request->reqCuId;

        count = tx_request->reqSizeNBytes;
        net_src = calcBufferNetID(sys, buf_off, count);
        net_dst = calcCUNetID(sys, cu);

        DPRINTF("Handling chunk of status SrcBufToSrcCU_Data %s\n", "");

    } else if (chunk_status == AllChunkStates::SrcCUToSrcBuf_ReqCmd) {

        // parse out relevant tx information
        Sys_ID sys = tx_request->srcRank;
        Buf_Offset buf_off = tx_request->bufByteOff;
        CU_ID cu = tx_request->reqCuId;

        // TODO parameterize better?
        count = req_cmd_size;
        net_src = calcCUNetID(sys, cu);
        net_dst = calcBufferNetID(sys, buf_off, count);

        DPRINTF("Handling chunk of status SrcCUToSrcBuf_ReqCmd %s\n", "");

    } else if (chunk_status == AllChunkStates::SrcCUToSrcBuf_Data) {

        // parse out relevant tx information
        Sys_ID sys = tx_request->srcRank;
        Buf_Offset buf_off = tx_request->bufByteOff;
        CU_ID cu = tx_request->reqCuId;

        count = tx_request->reqSizeNBytes;
        net_src = calcCUNetID(sys, cu);
        net_dst = calcBufferNetID(sys, buf_off, count);

        DPRINTF("Handling chunk of status SrcCUToSrcBuf_Data %s\n", "");

    } else if (chunk_status == AllChunkStates::SrcToDstGPU_Data) {
        // Plain rank-to-rank Send: no buffer/CU addressing, straight from the
        // source rank's network endpoint to the destination's.
        Sys_ID tx_sys = tx_request->srcRank;
        Sys_ID rx_sys = tx_request->dstRank;

        count = tx_request->reqSizeNBytes;
        net_src = calcCUNetID(tx_sys, 0);
        net_dst = calcCUNetID(rx_sys, 0);

        DPRINTF("Handling chunk of status SrcToDstGPU_Data %s\n", "");

    } else {
        // TODO print info on that illegal state
        // error out for now
        THROW_ERR("Unreachable/illegal chunk state %s\n", "");
    }

    // if buffers not modeled then assume self and transmission will be
    // instantaneous or constant latency
    assert(net_src != -1 && net_dst != -1);

    // simply try transmission. handles ticketing if required. does not post
    // to pending_chunks_queue
    int constraining_resource =
        try_to_create_and_schedule_chunk(net_req_id, count, net_src, net_dst);

    if (constraining_resource == -1) {
        entry->set_transmission_tick(sim_get_time().time_val);
    }

    DPRINTF(
        "[handle_network_transmission] net_req_id=%u waiting on resource %d\n",
        net_req_id, constraining_resource);

    return constraining_resource;
}

// static
void CommonNetworkApi::advance_current_chunk(
    const RequestID net_req_id) noexcept {

    auto& local_chunk_tracker = CommonNetworkApi::get_chunk_tracker();

    auto& local_pending_chunks_queue =
        CommonNetworkApi::get_pending_chunks_queue();

    // 1) advance FSM of current
    // 2) handle next steps
    //      likely either transmission or working with MPI send/recv tracking
    // 3) handle failure to send (resources)

    // 1) the current is now done (yay)
    //  a) transition
    //  b) try to continue progress
    auto entry = local_chunk_tracker.search_entry(net_req_id);
    assert(entry.has_value());  // entry must exist

    Sys_ID related_sys = entry.value()->get_original_system_id();
    auto related_net_ptr = CommonNetworkApi::sys_to_net_ptr_map.at(related_sys);

    // clang-format off

    #ifdef VERBOSE
        std::cout << std::setw(10) << std::right
            << "_unknown_" << ": [_static_FE_]"
            << " advancing chunk"
            << " req_id " << net_req_id
            << std::endl;
    #endif

    // clang-format on

    // update req time stats
    AllChunkStates chunk_status = entry.value()->get_current_state();
    auto cur_tick = related_net_ptr->sim_get_time().time_val;
    auto prev_tick = entry.value()->get_transmission_tick();
    Tick req_lat = cur_tick - prev_tick;
    related_net_ptr->stat_total_req_ticks += (uint64_t)req_lat;
    switch (chunk_status) {
    case AllChunkStates::SrcCUToSrcBuf_ReqCmd:
        related_net_ptr->stat_histo_noc_mem_req.sample(req_lat);
        break;
    case AllChunkStates::SrcBufToSrcCU_Data:
        related_net_ptr->stat_histo_noc_mem_resp.sample(req_lat);
        break;
    case AllChunkStates::SrcCUToDstBuf_Data:
        related_net_ptr->stat_histo_remote_mem_st.sample(req_lat);
        break;
    case AllChunkStates::DstBufToSrcCU_AckCtrl:
        related_net_ptr->stat_histo_remote_mem_ack.sample(req_lat);
        break;
    default:
        // simply don't track stats if pkt type unknown
        break;
    }

    // update chunk FSM
    entry.value()->transition_next_state();

    // 2)
    int constraining_resource =
        related_net_ptr->handle_network_transception(net_req_id);

    // clang-format off

    #ifdef VERBOSE
        std::cout << std::setw(10) << std::right
            << "_unknown_" << ": [_static_FE_]"
            << " advancing chunk"
            << " more work is dependent on resource : "
            << constraining_resource
            <<std::endl;
    #endif

    // clang-format on

    // 3)
    if (constraining_resource != -1) {
        // failure. requeue
        local_pending_chunks_queue[constraining_resource].push(net_req_id);
    }
}

// static
bool CommonNetworkApi::handle_resources_pending_chunks(
    ResourceID freed_resource) noexcept {

    auto& local_pending_chunks_queue_ref =
        CommonNetworkApi::get_pending_chunks_queue();
    auto& local_chunk_tracker = CommonNetworkApi::get_chunk_tracker();

    // exit early if nothing waiting on this resource
    if (local_pending_chunks_queue_ref.count(freed_resource) == 0) {

        // clang-format off

        #ifdef VERBOSE
                std::cout << "Nothing depends on " << freed_resource << std::endl;
        #endif

        // clang-format on
        return false;
    }

    // idea: loop through the pending chunks waiting on the freed resource
    //
    // while freed resource is not consumed && pending chunks
    //  pop
    //  try to send
    //  if (new, different) constraining then add to that pending queue
    //  else (successful send) mark as consumed (breaks while)

    // Point of intrigue
    //  when a chunk arrives multiple resources may be free
    //  on retrying other freed resources this freed resource may be consumed
    //  thus when looping things may be blocked by this freed resource!
    //  solution: make a copy of the queue and iterate over that

    auto& pending_chunks_queue_ref =
        local_pending_chunks_queue_ref.at(freed_resource);
    auto pending_chunks_queue_copy = pending_chunks_queue_ref;

    // var to return info to caller
    bool freed_consumed = false;
    while (!pending_chunks_queue_copy.empty() && !freed_consumed) {
        // pull from local queue
        auto dependent_net_req_id = pending_chunks_queue_copy.front();

        // pop from both
        pending_chunks_queue_ref.pop();
        pending_chunks_queue_copy.pop();

        // get chunk and ask for originating system
        auto entry = local_chunk_tracker.search_entry(dependent_net_req_id);
        assert(entry.has_value());  // entry must exist
        Sys_ID related_sys = entry.value()->get_original_system_id();
        auto related_net_ptr =
            CommonNetworkApi::sys_to_net_ptr_map.at(related_sys);

        // retry
        int constraining_resource =
            related_net_ptr->handle_network_transception(dependent_net_req_id);

        // clang-format off

        #ifdef VERBOSE
            std::cout << "Freed resource " << freed_resource<<". Trying req_id " << dependent_net_req_id << " from system "
                    << related_sys << std::endl;
            std::cout << "Adding request " << dependent_net_req_id
                    << " as dependent on resource " << constraining_resource
                    << std::endl;
        #endif

        // clang-format on

        if (constraining_resource == -1) {
            freed_consumed = true;
        } else {
            // push to global queue
            local_pending_chunks_queue_ref[constraining_resource].push(
                dependent_net_req_id);
        }
    }

    // clean up if now empty
    if (local_pending_chunks_queue_ref.at(freed_resource).empty()) {
        local_pending_chunks_queue_ref.erase(freed_resource);
    }

    if (freed_consumed) {
        return true;
    }
    return false;
}

// static
void CommonNetworkApi::handle_resources_rejected_requests(
    ResourceID freed_resource) noexcept {

    auto local_topology = get_topology();
    auto& local_rejected_requests_queue_ref =
        CommonNetworkApi::get_rejected_requests_queue();
    auto& local_chunk_tracker = CommonNetworkApi::get_chunk_tracker();

    // exit early if nothing waiting on this resource
    if (local_rejected_requests_queue_ref.count(freed_resource) == 0) {
        return;
    }

    // this logic is tricky...
    // retry all or just one? Try hard to selectively retry
    // since success is unknown until later (event process) then technically all
    // should be retried... or else things could hang? Like there is some
    // pattern of resources and requests that would leave a resource never
    // freeing (ie it is not in use) but some request is in that key in the
    // tracker

    // even trickier...
    // the freed rejected requests may be for a different system/GPU and thus
    // won't be in this system's member variable for callbacks. must use map of
    // request IDs to systems

    // get, do NOT erase
    auto rejected_request_id_queue =
        local_rejected_requests_queue_ref.at(freed_resource);
    // retry ALL
    while (!rejected_request_id_queue.empty()) {

        auto rejected_request_id = rejected_request_id_queue.front();

        // clang-format off

        #ifdef VERBOSE
            std::cout << "Retrieving cb for " << rejected_request_id << std::endl;
        #endif

        // clang-format on

        // get chunk and ask for originating system
        auto entry = local_chunk_tracker.search_entry(rejected_request_id);
        assert(entry.has_value());  // entry must exist
        Sys_ID related_sys = entry.value()->get_original_system_id();

        // remove as new sim_cmd will be called and create new entry
        local_chunk_tracker.pop_entry(rejected_request_id);
        assert(!local_chunk_tracker.search_entry(rejected_request_id)
                    .has_value());  // assert deletion

        auto related_net_ptr =
            CommonNetworkApi::sys_to_net_ptr_map.at(related_sys);
        auto cb_func = related_net_ptr->retrieve_callback(rejected_request_id);
        const auto immediate_delta = timespec_t({PS, 0});
        related_net_ptr->sim_schedule(immediate_delta, cb_func.first,
                                      cb_func.second);

        rejected_request_id_queue.pop();
    }
    // now erase
    local_rejected_requests_queue_ref.erase(freed_resource);
    return;
}

// static
void CommonNetworkApi::process_chunk_arrival(void* args) noexcept {
    assert(args != nullptr);

    // parse chunk data
    auto* const data = static_cast<RequestID*>(args);
    assert(data);
    const auto net_req_id = *data;
    // clean up that heap object created during chunk generation
    delete data;

    // static so get member/class/static vars
    auto local_topology = get_topology();
    auto& local_resource_tracker = CommonNetworkApi::get_resource_tracker();

    // Okay order is important for fairness
    // if resources are freed and the (current, arrived) chunk is directly
    // continued then it will claim resources over pending chunks (not fair)
    // therefore
    // 1) get and hold onto the free resources
    // 2) advance current chunk
    //      may be placed in pending_chunks_queue
    // 3) release those freed resources
    // 4) retry pending chunks and rejected requests

    // 1) get what is freed
    std::vector<ResourceID> freed_resources = {};
    if (local_topology->is_ticketed_topology()) {
        // get and ERASE
        freed_resources = local_resource_tracker[net_req_id];
        local_resource_tracker.erase(net_req_id);
        assert(local_resource_tracker.count(net_req_id) ==
               0);  // assert destructive consumption
    }

    // 2) advance arrived chunk
    advance_current_chunk(net_req_id);

    // 3) free in backend
    if (local_topology->is_ticketed_topology()) {
        // freed_resources defined in step 1)
        local_topology->relinquish_resources(freed_resources);

        // clang-format off

        #ifdef VERBOSE
            std::cout << "Freeing resources :"<<std::endl;
            for(auto const fr : freed_resources){
                std::cout<<fr<<", ";
            }
            std::cout << std::endl;
        #endif

        // clang-format on
    }

    // 4) handle resources
    //  a) retry in network chunks
    //  b) retry out of network requests
    if (local_topology->is_ticketed_topology()) {
        for (auto freed_resource : freed_resources) {

            // clang-format off

            #ifdef VERBOSE
                std::cout << "Handling resource "<<freed_resource<<std::endl;
            #endif

            // clang-format on

            // 3)
            bool consumed = handle_resources_pending_chunks(freed_resource);

            // 4)
            // avoid trying when it's known to be gone now
            if (!consumed) {
                handle_resources_rejected_requests(freed_resource);
            }
        }
    }
}

int CommonNetworkApi::calcCUNetID(Sys_ID sys_id, CU_ID cu_id) noexcept {
    return CommonNetworkApi::topology->calc_cu_net_id(sys_id, cu_id);
}

int CommonNetworkApi::calcBufferNetID(Sys_ID sys_id,
                                      Buf_Offset buff_off,
                                      Req_Size req_size) noexcept {
    return CommonNetworkApi::topology->calc_buffer_net_id(sys_id, buff_off,
                                                          req_size);
}

double CommonNetworkApi::get_BW_at_dimension(const int dim) const {
    assert(0 <= dim && dim < dims_count);

    // return bandwidth of the requested dimension
    return bandwidth_per_dim[dim];
}

const std::string& CommonNetworkApi::name() const {
    return net_api_name;
}

Tick CommonNetworkApi::curTick() const {
    return sim_get_time().time_val;
}
