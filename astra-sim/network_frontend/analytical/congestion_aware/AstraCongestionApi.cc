/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "AstraCongestionApi.hh"
#include <cassert>
#include <iostream>

using namespace Congestion;

std::shared_ptr<EventQueue> AstraCongestionApi::event_queue;

std::shared_ptr<Topology> AstraCongestionApi::topology;

ChunkIdGenerator AstraCongestionApi::chunk_id_generator = {};

EventHandlerTracker AstraCongestionApi::event_handler_tracker = {};

EventHandlerTracker& AstraCongestionApi::get_event_handler_tracker() noexcept {
  return event_handler_tracker;
}

void AstraCongestionApi::process_chunk_arrival(void* args) noexcept {
  auto* data = static_cast<std::tuple<int, int, int, uint64_t, int>*>(args);
  auto [tag, src, dest, count, chunk_id] = *data;

  auto& tracker = AstraCongestionApi::get_event_handler_tracker();
  auto entry = tracker.search_entry(tag, src, dest, count, chunk_id);
  assert(entry.has_value()); // entry must exist

  if (entry.value()->both_callbacks_registered()) {
    // call both send and recv callback
    entry.value()->invoke_send_handler();
    entry.value()->invoke_recv_handler();

    // remove entry
    tracker.pop_entry(tag, src, dest, count, chunk_id);
  } else {
    // run only send callback, as recv is not ready yet.
    entry.value()->invoke_send_handler();

    // mark the transmission as finished
    entry.value()->set_transmission_finished();
  }
}

void AstraCongestionApi::link_event_queue(
    std::shared_ptr<EventQueue> event_queue) noexcept {
  AstraCongestionApi::event_queue = event_queue;
}

void AstraCongestionApi::link_topology(
    std::shared_ptr<Topology> topology) noexcept {
  AstraCongestionApi::topology = topology;
}

AstraCongestionApi::AstraCongestionApi(int rank) noexcept
    : AstraSim::AstraNetworkAPI(rank) {}

AstraCongestionApi::~AstraCongestionApi() noexcept = default;

// Divya: to port congestion backend to Chakra
// int AstraCongestionApi::sim_comm_size(AstraSim::sim_comm comm, int* size) {
//  return 0;
//}

// int AstraCongestionApi::sim_finish() {
//   return 0;
// }

// double AstraCongestionApi::sim_time_resolution() {
//   return 0;
// }

// int AstraCongestionApi::sim_init(AstraSim::AstraRemoteMemoryAPI* MEM) {
//   return 0;
// }

AstraSim::timespec_t AstraCongestionApi::sim_get_time() {
  auto current_time = event_queue->get_current_time();
  return {AstraSim::NS, current_time};
}

// Divya: to port congestion backend to Chakra
void AstraCongestionApi::sim_schedule(
    AstraSim::timespec_t delta,
    void (*fun_ptr)(void*),
    void* fun_arg) {
  // get current time
  auto event_time = event_queue->get_current_time();

  // calculate event time
  // assert(delta.time_res == AstraSim::NS); // fixme: assuming NS
  //std::cout << "[Before:] event_time: " << event_time << std::endl;
  event_time += delta.time_val;
  //std::cout << "[After:] event_time: " << event_time << std::endl;

  // schedule event
  event_queue->schedule_event(event_time, fun_ptr, fun_arg);
}

int AstraCongestionApi::sim_send(
    void* buffer,
    uint64_t count,
    int type,
    int dst,
    int tag,
    AstraSim::sim_request* request,
    void (*msg_handler)(void*),
    void* fun_arg) {
  // query chunk id
  auto src = sim_comm_get_rank();
  auto chunk_id =
      AstraCongestionApi::chunk_id_generator.get_send_id(tag, src, dst, count);

  // search tracker
  auto entry =
      event_handler_tracker.search_entry(tag, src, dst, count, chunk_id);
  if (entry.has_value()) {
    entry.value()->register_send_callback(msg_handler, fun_arg);
  } else {
    // create new entry and insert callback
    auto* new_entry =
        event_handler_tracker.create_new_entry(tag, src, dst, count, chunk_id);
    new_entry->register_send_callback(msg_handler, fun_arg);
  }

  // initiate transmission from src -> dst.
  auto chunk_arrival_arg = std::tuple(tag, src, dst, count, chunk_id);
  auto arg = std::make_unique<decltype(chunk_arrival_arg)>(chunk_arrival_arg);
  auto arg_ptr = static_cast<void*>(arg.release());
  auto route = std::move(topology->route(src, dst));

  auto chunk = std::make_unique<Chunk>(
      count, route, AstraCongestionApi::process_chunk_arrival, arg_ptr);
  topology->send(std::move(chunk));
}

int AstraCongestionApi::sim_recv(
    void* buffer,
    uint64_t count,
    int type,
    int src,
    int tag,
    AstraSim::sim_request* request,
    void (*msg_handler)(void*),
    void* fun_arg) {
  // query chunk id
  auto dst = sim_comm_get_rank();
  auto chunk_id =
      AstraCongestionApi::chunk_id_generator.get_recv_id(tag, src, dst, count);

  // search tracker
  auto entry =
      event_handler_tracker.search_entry(tag, src, dst, count, chunk_id);
  if (entry.has_value()) {
    // send() already invoked
    // behavior is decided whether the transmission is already finished or not
    if (entry.value()->is_transmission_finished()) {
      // pop entry
      event_handler_tracker.pop_entry(tag, src, dst, count, chunk_id);

      // run recv callback immediately
      auto delta = AstraSim::timespec_t{AstraSim::NS, 0};
      // sim_schedule(delta, msg_handler, fun_arg);
      //  Divya: Changes to port congestion backend to Chakra
      sim_schedule(delta, msg_handler, fun_arg);
    } else {
      // register recv callback
      entry.value()->register_recv_callback(msg_handler, fun_arg);
    }
  } else {
    // send() not yet called
    // create new entry and insert callback
    auto* new_entry =
        event_handler_tracker.create_new_entry(tag, src, dst, count, chunk_id);
    new_entry->register_recv_callback(msg_handler, fun_arg);
  }
}

// Divya: to port congestion backend to Chakra
// void AstraCongestionApi::pass_front_end_report(
//    AstraSim::AstraSimDataAPI astraSimDataAPI) {
//  // todo: implement
//}

// double AstraCongestionApi::get_BW_at_dimension(int dim) {
//   // todo: implement
//   // this is for Themis usage
// }
