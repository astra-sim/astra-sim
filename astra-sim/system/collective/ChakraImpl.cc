/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include <stdlib.h>
#include <unistd.h>

#include "astra-sim/system/collective/ChakraImpl.hh"
#include "extern/graph_frontend/chakra/et_feeder/et_feeder.h"
#include "astra-sim/system/RecvPacketEventHandlerData.hh"
#include "astra-sim/system/SendPacketEventHandlerData.hh"

using namespace std;
using namespace AstraSim;
using namespace Chakra;

typedef ChakraProtoMsg::NodeType ChakraNodeType;

HardwareResourceChakra::HardwareResourceChakra()
    : num_in_flight_cpu_ops(0), num_in_flight_gpu_comm_ops(0), 
    num_in_flight_gpu_comp_ops(0) {}

void HardwareResourceChakra::occupy(const shared_ptr<Chakra::ETFeederNode> node) {
  if (node->is_cpu_op()) {
    //assert(num_in_flight_cpu_ops == 0);
    ++num_in_flight_cpu_ops;
  } else {
    if (node->type() == ChakraNodeType::COMP_NODE){
      //assert(num_in_flight_gpu_comp_ops == 0);
      ++num_in_flight_gpu_comp_ops;
    } else{
      if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
        return; 
      }
      //assert(num_in_flight_gpu_comm_ops == 0);
      ++num_in_flight_gpu_comm_ops;
      }
  }
}

void HardwareResourceChakra::release(const shared_ptr<Chakra::ETFeederNode> node) {
  if (node->is_cpu_op()) {
    --num_in_flight_cpu_ops;
    //assert(num_in_flight_cpu_ops == 0);
  } else {
    if (node->type() == ChakraNodeType::COMP_NODE){
      --num_in_flight_gpu_comp_ops;
      //assert(num_in_flight_gpu_comp_ops == 0);
    } else {
      if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
        return;
      }
      --num_in_flight_gpu_comm_ops;
      //assert(num_in_flight_gpu_comm_ops == 0);
    }
  }
}

bool HardwareResourceChakra::is_available(
    const shared_ptr<Chakra::ETFeederNode> node) const {
      return true;
  if (node->is_cpu_op()) {
      if (num_in_flight_cpu_ops == 0) {
        return true;
      } else {
        return false;
      }
  } else {
      if (node->type() == ChakraNodeType::COMP_NODE){
        if (num_in_flight_gpu_comp_ops == 0) {
          return true;
        } else {
          return false;
        }
      } else {
        if (node->type() == ChakraNodeType::COMM_RECV_NODE){
          return true;
        }
        if (num_in_flight_gpu_comm_ops == 0) {
          return true;
        } else {
          return false;
        }
      }
  }
}


ChakraImpl::ChakraImpl(
    std::string et_filename,
    int id)
    : Algorithm() {
  this->et_feeder = new Chakra::ETFeeder(et_filename);
  this->id = id;
  this->hw_resource = new HardwareResourceChakra();
}

void ChakraImpl::issue(shared_ptr<Chakra::ETFeederNode> node) {
  ChakraNodeType type = node->type();
  hw_resource->occupy(node);
  if (type == ChakraNodeType::COMM_SEND_NODE) {
    sim_request snd_req;
    snd_req.srcRank = node->comm_src();
    snd_req.dstRank = node->comm_dst();
    snd_req.reqType = UINT8;
    SendPacketEventHandlerData* sehd = new SendPacketEventHandlerData;
    sehd->callable = this;
    sehd->wlhd = new WorkloadLayerHandlerData;
    sehd->wlhd->node_id = node->id();
    sehd->event = EventType::PacketSent;
    stream->owner->front_end_sim_send(
        0,
        Sys::dummy_data,
        // Note that we're using the comm size as hardcoded in the Impl Chakra et, ed through the comm. api, 
        // and ignore the comm.size fed in the workload chakra et. TODO: fix. 
        node->comm_size(),
        UINT8,
        node->comm_dst(),
        node->comm_tag(),
        &snd_req,
        &Sys::handleEvent,
        sehd);
  } else if (type == ChakraNodeType::COMM_RECV_NODE) {
    //cout << "at gpu " << id << "issue recv node for node " << node->id() << endl;
    sim_request rcv_req;
    RecvPacketEventHandlerData* rcehd = new RecvPacketEventHandlerData;
    rcehd->wlhd = new WorkloadLayerHandlerData;
    rcehd->wlhd->node_id = node->id();
    rcehd->chakra = this;
    rcehd->event = EventType::PacketReceived;
    stream->owner->front_end_sim_recv(
        0,
        Sys::dummy_data,
        node->comm_size(),
        UINT8,
        node->comm_src(),
        node->comm_tag(),
        &rcv_req,
        &Sys::handleEvent,
        rcehd);
  } else if (type == ChakraNodeType::COMP_NODE) {
    // if (id == 0)
    //   cout << "at gpu " << id << "issue comp node for node " << node->id() << endl;
    WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
    wlhd->node_id = node->id();
    uint64_t runtime = 1ul;
    if (node->runtime() != 0ul)
      // chakra runtimes are in microseconds and we should convert it into
      // nanoseconds
      runtime = node->runtime() * 1000;
    stream->owner->register_event(this, EventType::General, wlhd, runtime);
  }
}

void ChakraImpl::issue_dep_free_nodes() {
  shared_ptr<Chakra::ETFeederNode> node = et_feeder->getNextIssuableNode();
  while (node != nullptr) {
    issue(node);
    node = et_feeder->getNextIssuableNode();
  }
}

void ChakraImpl::call(EventType event, CallData* data) {
  if (data == nullptr) {
    issue_dep_free_nodes();
  } else {
    WorkloadLayerHandlerData* wlhd = (WorkloadLayerHandlerData*)data;
    shared_ptr<Chakra::ETFeederNode> node =
      et_feeder->lookupNode(wlhd->node_id);
    hw_resource->release(node);
    et_feeder->freeChildrenNodes(wlhd->node_id);
    issue_dep_free_nodes();
    et_feeder->removeNode(wlhd->node_id);
    delete wlhd;
  }
  if (!et_feeder->hasNodesToIssue()) {
    exit();
  }
}

void ChakraImpl::run(EventType event, CallData* data) {
  issue_dep_free_nodes();
  // cout << "Hello for " << id <<  endl;
}
