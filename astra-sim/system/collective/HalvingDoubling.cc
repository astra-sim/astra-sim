/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/collective/HalvingDoubling.hh"

#include <cmath>
#include <iostream>

#include "astra-sim/system/PacketBundle.hh"
#include "astra-sim/system/RecvPacketEventHandlerData.hh"

using namespace AstraSim;

HalvingDoubling::HalvingDoubling(
    ComType type,
    int id,
    RingTopology* ring_topology,
    uint64_t data_size)
    : Algorithm() {
  this->comType = type;
  this->id = id;
  this->logical_topo = ring_topology;
  this->data_size = data_size;
  this->nodes_in_ring = ring_topology->get_nodes_in_ring();
  this->parallel_reduce = 1;
  this->total_packets_sent = 0;
  this->total_packets_received = 0;
  this->free_packets = 0;
  this->zero_latency_packets = 0;
  this->non_zero_latency_packets = 0;
  this->toggle = false;
  this->name = Name::HalvingDoubling;
  if (ring_topology->get_dimension() == RingTopology::Dimension::Local) {
    transmition = MemBus::Transmition::Fast;
  } else {
    transmition = MemBus::Transmition::Usual;
  }
  switch (type) {
    case ComType::All_Reduce:
      stream_count = 2 * log2(nodes_in_ring);
      break;
    default:
      stream_count = log2(nodes_in_ring);
  }
  if (type == ComType::All_Gather) {
    max_count = 0;
  } else {
    max_count = log2(nodes_in_ring);
  }
  remained_packets_per_message = 1;
  remained_packets_per_max_count = 1;
  switch (type) {
    case ComType::All_Reduce:
      this->final_data_size = data_size;
      this->msg_size = data_size / 2;
      this->rank_offset = 1;
      this->offset_multiplier = 2;
      break;
    case ComType::All_Gather:
      this->final_data_size = data_size * nodes_in_ring;
      this->msg_size = data_size;
      this->rank_offset = nodes_in_ring / 2;
      this->offset_multiplier = 0.5;
      break;
    case ComType::Reduce_Scatter:
      this->final_data_size = data_size / nodes_in_ring;
      this->msg_size = data_size / 2;
      this->rank_offset = 1;
      this->offset_multiplier = 2;
      break;
    default:
      std::cerr
          << "######### Exiting because of unknown communication type for HalvingDoubling collective algorithm #########"
          << std::endl;
      std::exit(1);
  }
  RingTopology::Direction direction = specify_direction();
  this->curr_receiver = id;
  for (int i = 0; i < rank_offset; i++) {
    this->curr_receiver =
        ring_topology->get_receiver(this->curr_receiver, direction);
    this->curr_sender = curr_receiver;
  }
}

int HalvingDoubling::get_non_zero_latency_packets() {
  return log2(nodes_in_ring) - 1 * parallel_reduce;
}

RingTopology::Direction HalvingDoubling::specify_direction() {
  if (rank_offset == 0) {
    return RingTopology::Direction::Clockwise;
  }
  int reminder =
      (((RingTopology*)logical_topo)->get_index_in_ring() / rank_offset) % 2;
  if (reminder == 0) {
    return RingTopology::Direction::Clockwise;
  } else {
    return RingTopology::Direction::Anticlockwise;
  }
}

void HalvingDoubling::run(EventType event, CallData* data) {
  if (event == EventType::General) {
    free_packets += 1;
    ready();
    iteratable();
  } else if (event == EventType::PacketReceived) {
    total_packets_received++;
    insert_packet(nullptr);
  } else if (event == EventType::StreamInit) {
    for (int i = 0; i < parallel_reduce; i++) {
      insert_packet(nullptr);
    }
  }
}

void HalvingDoubling::release_packets() {
  for (auto packet : locked_packets) {
    packet->set_notifier(this);
  }
  if (NPU_to_MA == true) {
    (new PacketBundle(
         stream->owner,
         stream,
         locked_packets,
         processed,
         send_back,
         msg_size,
         transmition))
        ->send_to_MA();
  } else {
    (new PacketBundle(
         stream->owner,
         stream,
         locked_packets,
         processed,
         send_back,
         msg_size,
         transmition))
        ->send_to_NPU();
  }
  locked_packets.clear();
}

void HalvingDoubling::process_stream_count() {
  if (remained_packets_per_message > 0) {
    remained_packets_per_message--;
  }
  if (id == 0) {
  }
  if (remained_packets_per_message == 0 && stream_count > 0) {
    stream_count--;
    if (stream_count > 0) {
      remained_packets_per_message = 1;
    }
  }
  if (remained_packets_per_message == 0 && stream_count == 0 &&
      stream->state != StreamState::Dead) {
    stream->changeState(StreamState::Zombie);
  }
}

void HalvingDoubling::process_max_count() {
  if (remained_packets_per_max_count > 0)
    remained_packets_per_max_count--;
  if (remained_packets_per_max_count == 0) {
    max_count--;
    release_packets();
    remained_packets_per_max_count = 1;
    rank_offset *= offset_multiplier;
    msg_size /= offset_multiplier;
    if (rank_offset == nodes_in_ring && comType == ComType::All_Reduce) {
      offset_multiplier = 0.5;
      rank_offset *= offset_multiplier;
      msg_size /= offset_multiplier;
    }
    curr_receiver = id;
    RingTopology::Direction direction = specify_direction();
    for (int i = 0; i < rank_offset; i++) {
      curr_receiver = ((RingTopology*)logical_topo)->get_receiver(curr_receiver, direction);
      curr_sender = curr_receiver;
    }
  }
}

void HalvingDoubling::reduce() {
  process_stream_count();
  packets.pop_front();
  free_packets--;
  total_packets_sent++;
}

bool HalvingDoubling::iteratable() {
  if (stream_count == 0 &&
      free_packets == (parallel_reduce * 1)) { // && not_delivered==0
    exit();
    return false;
  }
  return true;
}

void HalvingDoubling::insert_packet(Callable* sender) {
  if (zero_latency_packets == 0 && non_zero_latency_packets == 0) {
    zero_latency_packets = parallel_reduce * 1;
    non_zero_latency_packets =
        get_non_zero_latency_packets(); //(nodes_in_ring-1)*parallel_reduce*1;
    toggle = !toggle;
  }
  if (zero_latency_packets > 0) {
    packets.push_back(MyPacket(
        msg_size,
        stream->current_queue_id,
        curr_sender,
        curr_receiver)); // vnet Must be changed for alltoall topology
    packets.back().sender = sender;
    locked_packets.push_back(&packets.back());
    processed = false;
    send_back = false;
    NPU_to_MA = true;
    process_max_count();
    zero_latency_packets--;
    return;
  } else if (non_zero_latency_packets > 0) {
    packets.push_back(MyPacket(
        msg_size,
        stream->current_queue_id,
        curr_sender,
        curr_receiver)); // vnet Must be changed for alltoall topology
    packets.back().sender = sender;
    locked_packets.push_back(&packets.back());
    if (comType == ComType::Reduce_Scatter ||
        (comType == ComType::All_Reduce && toggle)) {
      processed = true;
    } else {
      processed = false;
    }
    if (non_zero_latency_packets <= parallel_reduce * 1) {
      send_back = false;
    } else {
      send_back = true;
    }
    NPU_to_MA = false;
    process_max_count();
    non_zero_latency_packets--;
    return;
  }
  Sys::sys_panic("should not inject nothing!");
}

bool HalvingDoubling::ready() {
  if (stream->state == StreamState::Created ||
      stream->state == StreamState::Ready) {
    stream->changeState(StreamState::Executing);
  }
  if (packets.size() == 0 || stream_count == 0 ||
      free_packets == 0) {
    return false;
  }
  MyPacket packet = packets.front();
  sim_request snd_req;
  snd_req.srcRank = id;
  snd_req.dstRank = packet.preferred_dest;
  snd_req.tag = stream->stream_id;
  snd_req.reqType = UINT8;
  snd_req.vnet = this->stream->current_queue_id;
  stream->owner->front_end_sim_send(
      0,
      Sys::dummy_data,
      packet.msg_size,
      UINT8,
      packet.preferred_dest,
      stream->stream_id,
      &snd_req,
      &Sys::handleEvent,
      nullptr); // stream_id+(packet.preferred_dest*50)
  sim_request rcv_req;
  rcv_req.vnet = this->stream->current_queue_id;
  RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
      stream,
      stream->owner->id,
      EventType::PacketReceived,
      packet.preferred_vnet,
      packet.stream_id);
  stream->owner->front_end_sim_recv(
      0,
      Sys::dummy_data,
      packet.msg_size,
      UINT8,
      packet.preferred_src,
      stream->stream_id,
      &rcv_req,
      &Sys::handleEvent,
      ehd); // stream_id+(owner->id*50)
  reduce();
  return true;
}

void HalvingDoubling::exit() {
  if (packets.size() != 0) {
    packets.clear();
  }
  if (locked_packets.size() != 0) {
    locked_packets.clear();
  }
  stream->owner->proceed_to_next_vnet_baseline((StreamBaseline*)stream);
}
