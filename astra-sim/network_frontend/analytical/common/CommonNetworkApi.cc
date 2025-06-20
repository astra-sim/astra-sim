/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/CommonNetworkApi.hh"
#include <cassert>

using namespace AstraSim;
using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;

std::shared_ptr<EventQueue> CommonNetworkApi::event_queue = nullptr;

ChunkIdGenerator CommonNetworkApi::chunk_id_generator = {};

CallbackTracker CommonNetworkApi::callback_tracker = {};

int CommonNetworkApi::dims_count = -1;

std::vector<Bandwidth> CommonNetworkApi::bandwidth_per_dim = {};

void CommonNetworkApi::set_event_queue(
    std::shared_ptr<EventQueue> event_queue_ptr) noexcept {
    assert(event_queue_ptr != nullptr);

    CommonNetworkApi::event_queue = std::move(event_queue_ptr);
}

CallbackTracker& CommonNetworkApi::get_callback_tracker() noexcept {
    return callback_tracker;
}

void CommonNetworkApi::process_chunk_arrival(void* args) noexcept {
    assert(args != nullptr);

    // parse chunk data
    auto* const data =
        static_cast<std::tuple<int, int, int, uint64_t, int>*>(args);
    const auto [tag, src, dest, count, chunk_id] = *data;
    delete data;

    // search tracker
    auto& tracker = CommonNetworkApi::get_callback_tracker();
    const auto entry = tracker.search_entry(tag, src, dest, count, chunk_id);
    assert(entry.has_value());  // entry must exist

    // if both callbacks are registered, invoke both callbacks
    if (entry.value()->both_callbacks_registered()) {
        entry.value()->invoke_send_handler();
        entry.value()->invoke_recv_handler();

        // remove entry
        tracker.pop_entry(tag, src, dest, count, chunk_id);
    } else {
        // run only send callback, as recv is not ready yet.
        entry.value()->invoke_send_handler();

        // mark the transmission as finished
        // so that recv callback will be invoked immediately
        // when sim_recv() is called
        entry.value()->set_transmission_finished();
    }
}

CommonNetworkApi::CommonNetworkApi(const int rank) noexcept
    : AstraNetworkAPI(rank) {
    assert(rank >= 0);
}

timespec_t CommonNetworkApi::sim_get_time() {
    // get current time from event queue
    const auto current_time = event_queue->get_current_time();

    // return the current time in ASTRA-sim format
    const auto astra_sim_time = static_cast<double>(current_time);
    return {NS, astra_sim_time};
}

void CommonNetworkApi::sim_schedule(const timespec_t delta,
                                    void (*fun_ptr)(void*),
                                    void* const fun_arg) {
    assert(delta.time_res == NS);
    assert(fun_ptr != nullptr);

    // calculate absolute event time
    const auto current_time = sim_get_time();
    const auto event_time = current_time.time_val + delta.time_val;
    const auto event_time_ns = static_cast<EventTime>(event_time);

    // schedule the event to the event queue
    assert(event_time_ns >= event_queue->get_current_time());
    event_queue->schedule_event(event_time_ns, fun_ptr, fun_arg);
}

int CommonNetworkApi::sim_recv(void* const buffer,
                               const uint64_t count,
                               const int type,
                               const int src,
                               const int tag,
                               sim_request* const request,
                               void (*msg_handler)(void*),
                               void* const fun_arg) {
    // query chunk id
    const auto dst = sim_comm_get_rank();
    const auto chunk_id =
        CommonNetworkApi::chunk_id_generator.create_recv_chunk_id(tag, src, dst,
                                                                  count);

    // search tracker
    auto entry = callback_tracker.search_entry(tag, src, dst, count, chunk_id);
    if (entry.has_value()) {
        // send() already invoked
        // behavior is decided whether the transmission is already finished or
        // not
        if (entry.value()->is_transmission_finished()) {
            // transmission already finished, run callback immediately

            // pop entry
            callback_tracker.pop_entry(tag, src, dst, count, chunk_id);

            // run recv callback immediately
            const auto delta = timespec_t{NS, 0};
            sim_schedule(delta, msg_handler, fun_arg);
        } else {
            // transmission not finished yet, just register callback
            entry.value()->register_recv_callback(msg_handler, fun_arg);
        }
    } else {
        // send() not yet called
        // create new entry and insert callback
        auto* const new_entry =
            callback_tracker.create_new_entry(tag, src, dst, count, chunk_id);
        new_entry->register_recv_callback(msg_handler, fun_arg);
    }

    // return
    return 0;
}

double CommonNetworkApi::get_BW_at_dimension(const int dim) {
    assert(0 <= dim && dim < dims_count);

    // return bandwidth of the requested dimension
    return bandwidth_per_dim[dim];
}
