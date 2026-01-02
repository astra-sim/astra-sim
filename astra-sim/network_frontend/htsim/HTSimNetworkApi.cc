/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "HTSimNetworkApi.hh"
#include "astra-sim/common/Logging.hh"
#include <cassert>
#include <iostream>

#include "HTSimSession.hh"

using namespace AstraSim;
using namespace NetworkAnalytical;
using namespace HTSim;

std::shared_ptr<Topology> HTSimNetworkApi::topology;
std::shared_ptr<CompletionTracker> HTSimNetworkApi::completion_tracker;
unsigned HTSimNetworkApi::flow_id;

bool CompletionTracker::all_finished() {
    if (num_unfinished_ranks_ == 0) {
        return true;
    }
    return false;
}

void CompletionTracker::mark_rank_as_finished(int rank) {
    if (completion_tracker[rank] == 0) {
        completion_tracker[rank] = 1;
        num_unfinished_ranks_--;
    }
    if (num_unfinished_ranks_ == 0) {
        AstraSim::LoggerFactory::get_logger("network")->debug(
            "All ranks have finished. Exiting simulation.");
        auto& htsim_session = HTSimSession::instance();
        htsim_session.stop_simulation();
    }
}

void HTSimNetworkApi::set_topology(
    std::shared_ptr<Topology> topology_ptr) noexcept {
    assert(topology_ptr != nullptr);

    // move topology
    HTSimNetworkApi::topology = std::move(topology_ptr);

    // set topology-related values
    HTSimNetworkApi::dims_count = HTSimNetworkApi::topology->get_dims_count();
    HTSimNetworkApi::bandwidth_per_dim =
        HTSimNetworkApi::topology->get_bandwidth_per_dim();
}

void HTSimNetworkApi::set_completion_tracker(
    std::shared_ptr<CompletionTracker> completion_tracker_ptr) noexcept {
    assert(completion_tracker_ptr != nullptr);
    HTSimNetworkApi::completion_tracker = std::move(completion_tracker_ptr);
}

HTSimNetworkApi::HTSimNetworkApi(const int rank) noexcept
    : CommonNetworkApi(rank) {
    assert(rank >= 0);
}

AstraSim::timespec_t HTSimNetworkApi::sim_get_time() {
    auto& htsim_session = HTSimSession::instance();
    AstraSim::timespec_t timeSpec;
    timeSpec.time_res = AstraSim::NS;
    timeSpec.time_val = htsim_session.get_time_ns();
    // std::cout << "Time is now " << timeSpec.time_val / 1000.0 << std::endl;
    return timeSpec;
}

int HTSimNetworkApi::sim_send(void* const buffer,
                              const uint64_t count,
                              const int type,
                              const int dst,
                              const int tag,
                              sim_request* const request,
                              void (*msg_handler)(void*),
                              void* const fun_arg) {

    const auto src = sim_comm_get_rank();

    // save information about event for future
    flow_id++;
    auto flow_info = FlowInfo(src, dst, count, tag);

    // Trigger HTSim to schedule flow
    HTSimSession::instance().send_flow(flow_info, flow_id, msg_handler,
                                       fun_arg);
    // return
    return 0;
}

void HTSimNetworkApi::sim_schedule(timespec_t delta,
                                   void (*msg_handler)(void* fun_arg),
                                   void* fun_arg) {
    assert(delta.time_res == NS);
    auto when_ns = delta.time_val;
    HTSimSession::instance().schedule_astra_event(when_ns, msg_handler,
                                                  fun_arg);
}

int HTSimNetworkApi::sim_recv(void* const buffer,
                              const uint64_t message_size,
                              const int type,
                              const int src,
                              const int tag,
                              sim_request* const request,
                              void (*msg_handler)(void*),
                              void* const fun_arg) {
    // query chunk id
    const auto dst = sim_comm_get_rank();
    auto recv_event =
        MsgEvent(src, dst, Dir::Receive, message_size, fun_arg, msg_handler);
    MsgEventKey recv_event_key = std::make_pair(
        tag, std::make_pair(recv_event.src_id, recv_event.dst_id));
    if (HTSimSession::msg_standby.find(recv_event_key) !=
        HTSimSession::msg_standby.end()) {
        // HTSim received the message before sim_recv was called.
        int received_msg_bytes = HTSimSession::msg_standby[recv_event_key];
        if (received_msg_bytes == message_size) {
            // Message matches what we expected.
            HTSimSession::msg_standby.erase(recv_event_key);
            recv_event.callHandler();
        } else if (received_msg_bytes >= message_size) {
            // Node received more than it expected.
            // This can happen if the message is split into multiple chunks as
            // part of one collective phase, for example in Ring.
            HTSimSession::msg_standby[recv_event_key] -= message_size;
            recv_event.callHandler();
        } else {
            // Node received less than it expected.
            // Reduce the number of bytes we are waiting to receive.
            HTSimSession::msg_standby.erase(recv_event_key);
            recv_event.remaining_msg_bytes -= received_msg_bytes;
            HTSimSession::recv_waiting[recv_event_key] = recv_event;
        }
    } else {
        // HTSim has not yet received anything.
        if (HTSimSession::recv_waiting.find(recv_event_key) ==
            HTSimSession::recv_waiting.end()) {
            // We have not been expecting anything.
            HTSimSession::recv_waiting[recv_event_key] = recv_event;
        } else {
            // We have already been expecting something.
            // Increment the number of bytes we are waiting to receive.
            int expecting_msg_bytes =
                HTSimSession::recv_waiting[recv_event_key].remaining_msg_bytes;
            recv_event.remaining_msg_bytes += expecting_msg_bytes;
            HTSimSession::recv_waiting[recv_event_key] = recv_event;
        }
    }
    return 0;
}

void HTSimNetworkApi::sim_notify_finished() {
    // Let HTSim know the simulation should finish.
    // Set endtime to now.
    completion_tracker->mark_rank_as_finished(rank);
    return;
}
