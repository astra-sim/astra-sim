/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/collective/AllToAll.hh"

using namespace AstraSim;

AllToAll::AllToAll(
    ComType type, int window, int id, RingTopology* allToAllTopology,
    uint64_t data_size, RingTopology::Direction direction, InjectionPolicy injection_policy)
    : Ring(type, id, allToAllTopology, data_size, direction, injection_policy) {
  this->name = Name::AllToAll;
  this->middle_point = nodes_in_ring - 1;
  if (window == -1) {
    parallel_reduce = nodes_in_ring - 1;
  } else {
    parallel_reduce = (int)std::min(window, nodes_in_ring - 1);
  }
  if (type == ComType::All_to_All) {
    this->stream_count = nodes_in_ring - 1;
  }
}

void AllToAll::run(EventType event, CallData* data) {
  if (event == EventType::General) {
    free_packets += 1;
    if (comType == ComType::All_Reduce && stream_count <= middle_point) {
      if (total_packets_received < middle_point) {
        return;
      }
      for (int i = 0; i < parallel_reduce; i++) {
        ready();
      }
      iteratable();
    } else {
      ready();
      iteratable();
    }

  } else if (event == EventType::PacketReceived) {
    total_packets_received++;
    insert_packet(nullptr);

  } else if (event == EventType::StreamInit) {
    for (int i = 0; i < parallel_reduce; i++) {
      insert_packet(nullptr);
    }
  }
}

void AllToAll::process_max_count() {
  if (remained_packets_per_max_count > 0)
    remained_packets_per_max_count--;

  if (remained_packets_per_max_count == 0) {
    max_count--;
    release_packets();
    remained_packets_per_max_count = 1;

    curr_receiver = ((RingTopology*)logical_topo)->get_receiver(curr_receiver, direction);
    if (curr_receiver == id) {
      curr_receiver = ((RingTopology*)logical_topo)->get_receiver(curr_receiver, direction);
    }

    curr_sender = ((RingTopology*)logical_topo)->get_sender(curr_sender, direction);
    if (curr_sender == id) {
      curr_sender = ((RingTopology*)logical_topo)->get_sender(curr_sender, direction);
    }
  }
}

int AllToAll::get_non_zero_latency_packets() {
  if (((RingTopology*)logical_topo)->get_dimension() != RingTopology::Dimension::Local) {
    return parallel_reduce * 1;
  } else {
    return (nodes_in_ring - 1) * parallel_reduce * 1;
  }
}
