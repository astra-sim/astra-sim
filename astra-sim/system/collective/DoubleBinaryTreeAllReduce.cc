/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/collective/DoubleBinaryTreeAllReduce.hh"

#include "astra-sim/system/PacketBundle.hh"
#include "astra-sim/system/RecvPacketEventHandlerData.hh"

using namespace AstraSim;

DoubleBinaryTreeAllReduce::DoubleBinaryTreeAllReduce(
    int id,
    BinaryTree* tree,
    uint64_t data_size)
    : Algorithm() {
  this->id = id;
  this->logical_topo = tree;
  this->data_size = data_size;
  this->state = State::Begin;
  this->reductions = 0;
  this->parent = tree->get_parent_id(id);
  this->left_child = tree->get_left_child_id(id);
  this->right_child = tree->get_right_child_id(id);
  this->type = tree->get_node_type(id);
  this->final_data_size = data_size;
  this->comType = ComType::All_Reduce;
  this->name = Name::DoubleBinaryTree;
}

void DoubleBinaryTreeAllReduce::run(EventType event, CallData* data) {
  if (state == State::Begin && type == BinaryTree::Type::Leaf) { // leaf.1
    (new PacketBundle(
         stream->owner,
         stream,
         false,
         false,
         data_size,
         MemBus::Transmition::Usual))
        ->send_to_MA();
    state = State::SendingDataToParent;

  } else if (
      state == State::SendingDataToParent &&
      type == BinaryTree::Type::Leaf) { // leaf.3
    // sending
    sim_request snd_req;
    snd_req.srcRank = stream->owner->id;
    snd_req.dstRank = parent;
    snd_req.tag = stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET;
    snd_req.reqType = UINT8;
    snd_req.vnet = this->stream->current_queue_id;
    stream->owner->front_end_sim_send(
        0,
        Sys::dummy_data,
        data_size,
        UINT8,
        parent,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET,
        &snd_req,
        &Sys::handleEvent,
        nullptr);
    // receiving
    sim_request rcv_req;
    rcv_req.vnet = this->stream->current_queue_id;
    RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
        stream,
        stream->owner->id,
        EventType::PacketReceived,
        stream->current_queue_id,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET);
    stream->owner->front_end_sim_recv(
        0,
        Sys::dummy_data,
        data_size,
        UINT8,
        parent,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET,
        &rcv_req,
        &Sys::handleEvent,
        ehd);
    state = State::WaitingDataFromParent;

  } else if (
      state == State::WaitingDataFromParent &&
      type == BinaryTree::Type::Leaf) { // leaf.4
    (new PacketBundle(
         stream->owner,
         stream,
         false,
         false,
         data_size,
         MemBus::Transmition::Usual))
        ->send_to_NPU();
    state = State::End;

  } else if (state == State::End && type == BinaryTree::Type::Leaf) { // leaf.5
    exit();

  } else if (
      state == State::Begin &&
      type == BinaryTree::Type::Intermediate) { // int.1
    sim_request rcv_req;
    rcv_req.vnet = this->stream->current_queue_id;
    RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
        stream,
        stream->owner->id,
        EventType::PacketReceived,
        stream->current_queue_id,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET);
    stream->owner->front_end_sim_recv(
        0,
        Sys::dummy_data,
        data_size,
        UINT8,
        left_child,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET,
        &rcv_req,
        &Sys::handleEvent,
        ehd);
    sim_request rcv_req2;
    rcv_req2.vnet = this->stream->current_queue_id;
    RecvPacketEventHandlerData* ehd2 = new RecvPacketEventHandlerData(
        stream,
        stream->owner->id,
        EventType::PacketReceived,
        stream->current_queue_id,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET);
    stream->owner->front_end_sim_recv(
        0,
        Sys::dummy_data,
        data_size,
        UINT8,
        right_child,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET,
        &rcv_req2,
        &Sys::handleEvent,
        ehd2);
    state = State::WaitingForTwoChildData;

  } else if (
      state == State::WaitingForTwoChildData &&
      type == BinaryTree::Type::Intermediate &&
      event == EventType::PacketReceived) { // int.2
    (new PacketBundle(
         stream->owner,
         stream,
         true,
         false,
         data_size,
         MemBus::Transmition::Usual))
        ->send_to_NPU();
    state = State::WaitingForOneChildData;

  } else if (
      state == State::WaitingForOneChildData &&
      type == BinaryTree::Type::Intermediate &&
      event == EventType::PacketReceived) { // int.3
    (new PacketBundle(
         stream->owner,
         stream,
         true,
         true,
         data_size,
         MemBus::Transmition::Usual))
        ->send_to_NPU();
    state = State::SendingDataToParent;

  } else if (
      reductions < 1 && type == BinaryTree::Type::Intermediate &&
      event == EventType::General) { // int.4
    reductions++;

  } else if (
      state == State::SendingDataToParent &&
      type == BinaryTree::Type::Intermediate) { // int.5
    // sending
    sim_request snd_req;
    snd_req.srcRank = stream->owner->id;
    snd_req.dstRank = parent;
    snd_req.tag = stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET;
    snd_req.reqType = UINT8;
    snd_req.vnet = this->stream->current_queue_id;
    stream->owner->front_end_sim_send(
        0,
        Sys::dummy_data,
        data_size,
        UINT8,
        parent,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET,
        &snd_req,
        &Sys::handleEvent,
        nullptr);
    // receiving
    sim_request rcv_req;
    rcv_req.vnet = this->stream->current_queue_id;
    RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
        stream,
        stream->owner->id,
        EventType::PacketReceived,
        stream->current_queue_id,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET);
    stream->owner->front_end_sim_recv(
        0,
        Sys::dummy_data,
        data_size,
        UINT8,
        parent,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET,
        &rcv_req,
        &Sys::handleEvent,
        ehd);
    state = State::WaitingDataFromParent;

  } else if (
      state == State::WaitingDataFromParent &&
      type == BinaryTree::Type::Intermediate &&
      event == EventType::PacketReceived) { // int.6
    (new PacketBundle(
         stream->owner,
         stream,
         true,
         true,
         data_size,
         MemBus::Transmition::Usual))
        ->send_to_NPU();
    state = State::SendingDataToChilds;

  } else if (
      state == State::SendingDataToChilds &&
      type == BinaryTree::Type::Intermediate) {
    sim_request snd_req;
    snd_req.srcRank = stream->owner->id;
    snd_req.dstRank = left_child;
    snd_req.tag = stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET;
    snd_req.reqType = UINT8;
    snd_req.vnet = this->stream->current_queue_id;
    stream->owner->front_end_sim_send(
        0,
        Sys::dummy_data,
        data_size,
        UINT8,
        left_child,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET,
        &snd_req,
        &Sys::handleEvent,
        nullptr);
    sim_request snd_req2;
    snd_req2.srcRank = stream->owner->id;
    snd_req2.dstRank = left_child;
    snd_req2.tag = stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET;
    snd_req2.reqType = UINT8;
    snd_req2.vnet = this->stream->current_queue_id;
    stream->owner->front_end_sim_send(
        0,
        Sys::dummy_data,
        data_size,
        UINT8,
        right_child,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET,
        &snd_req2,
        &Sys::handleEvent,
        nullptr);
    exit();
    return;

  } else if (
      state == State::Begin && type == BinaryTree::Type::Root) { // root.1
    int only_child_id = left_child >= 0 ? left_child : right_child;
    sim_request rcv_req;
    rcv_req.vnet = this->stream->current_queue_id;
    RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
        stream,
        stream->owner->id,
        EventType::PacketReceived,
        stream->current_queue_id,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET);
    stream->owner->front_end_sim_recv(
        0,
        Sys::dummy_data,
        data_size,
        UINT8,
        only_child_id,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET,
        &rcv_req,
        &Sys::handleEvent,
        ehd);
    state = State::WaitingForOneChildData;

  } else if (
      state == State::WaitingForOneChildData &&
      type == BinaryTree::Type::Root) { // root.2
    (new PacketBundle(
         stream->owner,
         stream,
         true,
         true,
         data_size,
         MemBus::Transmition::Usual))
        ->send_to_NPU();
    state = State::SendingDataToChilds;
    return;

  } else if (
      state == State::SendingDataToChilds &&
      type == BinaryTree::Type::Root) { // root.2
    int only_child_id = left_child >= 0 ? left_child : right_child;
    sim_request snd_req;
    snd_req.srcRank = stream->owner->id;
    snd_req.dstRank = only_child_id;
    snd_req.tag = stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET;
    snd_req.reqType = UINT8;
    snd_req.vnet = this->stream->current_queue_id;
    stream->owner->front_end_sim_send(
        0,
        Sys::dummy_data,
        data_size,
        UINT8,
        only_child_id,
        stream->stream_id % this->TAG_OFFSET + this->TAG_OFFSET,
        &snd_req,
        &Sys::handleEvent,
        nullptr);
    exit();
    return;
  }
}
