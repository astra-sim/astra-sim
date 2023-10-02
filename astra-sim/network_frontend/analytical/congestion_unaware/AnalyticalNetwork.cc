/******************************************************************************
This source code is licensed under the MIT license found in the LICENSE file in
the root directory of this source tree.
 *******************************************************************************/

#include "AnalyticalNetwork.hh"

using namespace Analytical;

std::shared_ptr<EventQueue> AnalyticalNetwork::event_queue;

std::shared_ptr<Topology> AnalyticalNetwork::topology;

SendRecvTrackingMap AnalyticalNetwork::send_recv_tracking_map;

CostModel* AnalyticalNetwork::cost_model;

std::shared_ptr<PayloadSizeTracker> AnalyticalNetwork::payload_size_tracker;

AnalyticalNetwork::AnalyticalNetwork(int rank, int dims_count) noexcept
    : AstraSim::AstraNetworkAPI(rank), dims_count(dims_count) {
  payload_size_tracker = std::make_shared<PayloadSizeTracker>(dims_count);
}

int AnalyticalNetwork::sim_send(
    void* buffer,
    uint64_t count,
    int type,
    int dst,
    int tag,
    AstraSim::sim_request* request,
    void (*msg_handler)(void*),
    void* fun_arg) {
  // get source id
  auto src = sim_comm_get_rank();

  // compute send latency in ns
  // FIXME: if you want to use time_res other than NS
  AstraSim::timespec_t delta;
  delta.time_res = AstraSim::NS;
  auto used_dim = -1;
  std::tie(delta.time_val, used_dim) =
      topology->send(src, dst, count); // simulate src->dst and get latency

  // accumulate total message size
  if (src == 0) {
    payload_size_tracker->addPayloadSize(count, used_dim);
  }

  if (send_recv_tracking_map.has_recv_operation(tag, src, dst, count)) {
    // recv operation already issued.
    // Schedule both send and recv event handler.
    auto recv_event_handler =
        send_recv_tracking_map.pop_recv_event_handler(tag, src, dst, count);
    sim_schedule(delta, msg_handler, fun_arg);
    sim_schedule(
        delta,
        recv_event_handler.get_fun_ptr(),
        recv_event_handler.get_fun_arg());
  } else {
    // recv operation not issued yet.
    // Should assign this send operation to the tracker.

    // schedule send event
    sim_schedule(delta, msg_handler, fun_arg);

    // compute send finish time  // FIXME: if you want to use time_res other
    // than NS
    auto send_finish_time = sim_get_time();
    send_finish_time.time_val += delta.time_val;

    // schedule this into the tracker
    send_recv_tracking_map.insert_send(tag, src, dst, count, send_finish_time);
  }

  return 0;
}

int AnalyticalNetwork::sim_recv(
    void* buffer,
    uint64_t count,
    int type,
    int src,
    int tag,
    AstraSim::sim_request* request,
    void (*msg_handler)(void*),
    void* fun_arg) {
  // get source id
  auto dst = sim_comm_get_rank();

  if (send_recv_tracking_map.has_send_operation(tag, src, dst, count)) {
    // send operation already issued.
    // should compute delta
    AstraSim::timespec_t delta;
    delta.time_res = AstraSim::NS;

    auto current_time = sim_get_time();
    auto send_finish_time =
        send_recv_tracking_map.pop_send_finish_time(tag, src, dst, count);

    if (EventQueueEntry::compare_time_stamp(current_time, send_finish_time) <
        0) {
      // sent packet still inflight
      // schedule recv handler accordingly.
      delta.time_val = send_finish_time.time_val - current_time.time_val;
    } else {
      // send operation already finished.
      // invoke recv handler immediately
      delta.time_val = 0;
    }

    // schedule recv handler
    sim_schedule(delta, msg_handler, fun_arg);
  } else {
    // send operation not issued.
    // Add recv to the tracker and wait until corresponding sim_send to be
    // invoked.
    send_recv_tracking_map.insert_recv(
        tag, src, dst, count, msg_handler, fun_arg);
  }
  return 0;
}

void AnalyticalNetwork::sim_schedule(
    AstraSim::timespec_t delta,
    void (*fun_ptr)(void*),
    void* fun_arg) {
  // 1. compute event_time = current_time + delta
  auto event_time = sim_get_time();

  // FIXME: assuming time_res is always NS
  event_time.time_val += delta.time_val;

  // 2. schedule an event at the event_time
  event_queue->add_event(event_time, fun_ptr, fun_arg);
}

void AnalyticalNetwork::schedule(
    AstraSim::timespec_t event_time,
    void (*fun_ptr)(void*),
    void* fun_arg) {
  event_queue->add_event(event_time, fun_ptr, fun_arg);
}

AstraSim::timespec_t AnalyticalNetwork::sim_get_time() {
  return event_queue->get_current_time();
}

double AnalyticalNetwork::get_BW_at_dimension(int dim) {
  return AnalyticalNetwork::topology->getNpuTotalBandwidthPerDim(dim); // GB/s
}

void AnalyticalNetwork::set_event_queue(
    const std::shared_ptr<EventQueue>& event_queue_ptr) noexcept {
  AnalyticalNetwork::event_queue = event_queue_ptr;
}

void AnalyticalNetwork::set_topology(
    const std::shared_ptr<Topology>& topology_ptr) noexcept {
  AnalyticalNetwork::topology = topology_ptr;
}

void AnalyticalNetwork::set_cost_model(
    CostModel* const cost_model_ptr) noexcept {
  AnalyticalNetwork::cost_model = cost_model_ptr;
}
