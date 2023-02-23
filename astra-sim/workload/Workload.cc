/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/workload/Workload.hh"

#include "astra-sim/json.hpp"
#include "astra-sim/system/IntData.hh"
#include "astra-sim/system/MemEventHandlerData.hh"
#include "astra-sim/system/RecvPacketEventHandlerData.hh"
#include "astra-sim/system/SendPacketEventHandlerData.hh"
#include "astra-sim/system/WorkloadLayerHandlerData.hh"
#include <fstream>

#include <iostream>

using namespace std;
using namespace AstraSim;
using namespace Chakra;
using json = nlohmann::json;

typedef ChakraProtoMsg::NodeType ChakraNodeType;
typedef ChakraProtoMsg::CollectiveCommType ChakraCollectiveCommType;

Workload::Workload(
    Sys* sys,
    string eg_filename,
    string comm_group_filename) {
  this->sys = sys;
  if(!file_exists(eg_filename + "." + to_string(sys->id) + ".eg")){
    report();
    this->is_finished = true;
    this->eg_feeder= nullptr;
    return;
  }
  this->eg_feeder =
        new EGFeeder(eg_filename + "." + to_string(sys->id) + ".eg");
  // TODO: parametrize the number of available hardware resources
  this->hw_resource = new HardwareResource(1);
  initialize_comm_group(comm_group_filename);
  this->is_finished = false;
}

Workload::~Workload() {
  for (auto &cg:comm_groups)
    delete cg.second;
  if (this->eg_feeder != nullptr)
    delete this->eg_feeder;
}

void Workload::initialize_comm_group(string comm_group_filename)
{
  // communicator group input file is not given
  if (comm_group_filename.find("empty") != std::string::npos) {
    return;
  }

  ifstream inFile;
  json j;
  inFile.open(comm_group_filename);
  inFile >> j;

  for (json::iterator it = j.begin(); it != j.end(); ++it) {
    bool in_comm_group = false;

    for (auto id: it.value()) {
      if (id == sys->id) {
        in_comm_group = true;
      }
    }

    if (in_comm_group) {
      std::vector<int> involved_NPUs;
      for (auto id: it.value()) {
        involved_NPUs.push_back(id);
      }
      int comm_group_id=std::atoi(it.key().c_str());
      comm_groups[comm_group_id] = new CommunicatorGroup(comm_group_id, involved_NPUs, true, sys);
      // Note: All NPUs should create comm group with identical ids if they want to communicate with each other
    }
  }
}
CommunicatorGroup* Workload::get_comm_group(int id){
  if(comm_groups.find(id)==comm_groups.end()){
    return nullptr;
  }
  return comm_groups[id];
}

void Workload::issue_dep_free_nodes() {
  std::queue<shared_ptr<Chakra::EGFeederNode>> push_back_queue;
  shared_ptr<Chakra::EGFeederNode> node = eg_feeder->getNextIssuableNode();
  while (node != nullptr) {
    if (hw_resource->is_available(node)) {
      issue(node);
    } else {
      push_back_queue.push(node);
    }
    node = eg_feeder->getNextIssuableNode();
  }

  while (!push_back_queue.empty()) {
    shared_ptr<Chakra::EGFeederNode> node = push_back_queue.front();
    eg_feeder->pushBackIssuableNode(node->getChakraNode()->id());
    push_back_queue.pop();
  }
}

void Workload::issue(shared_ptr<Chakra::EGFeederNode> node) {
  if (node->getChakraNode()->node_type() == ChakraNodeType::COMP_NODE) {
    if (node->getChakraNode()->simulated_run_time() == 0) {
      skip_invalid(node);
    } else {
      if (sys->trace_enabled) {
        cout << "issue,sys->id=" << sys->id
          << ",tick=" << Sys::boostedTick()
          << ",node->id=" << node->getChakraNode()->id()
          << ",node->name=" << node->getChakraNode()->name() << endl;
      }
      issue_comp(node);
    }
  } else if ((node->getChakraNode()->node_type() == ChakraNodeType::COMM_COLL_NODE)
      || (node->getChakraNode()->node_type() == ChakraNodeType::COMM_SEND_NODE)
      || (node->getChakraNode()->node_type() == ChakraNodeType::COMM_RECV_NODE)) {
    if (sys->trace_enabled) {
      cout << "issue,sys->id=" << sys->id
        << ",tick=" << Sys::boostedTick()
        << ",node->id=" << node->getChakraNode()->id()
        << ",node->name=" << node->getChakraNode()->name() << endl;
    }
    issue_comm(node);
  } else if (node->getChakraNode()->node_type() == ChakraNodeType::INVALID_NODE) {
    skip_invalid(node);
  }
}

void Workload::issue_comp(shared_ptr<Chakra::EGFeederNode> node) {
  assert(node->getChakraNode()->node_type() == ChakraNodeType::COMP_NODE);
  hw_resource->occupy(node);

  WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
  wlhd->node_id = node->getChakraNode()->id();

  sys->register_event(
      this,
      EventType::General,
      wlhd,
      node->getChakraNode()->simulated_run_time());
}

void Workload::issue_comm(shared_ptr<Chakra::EGFeederNode> node) {
  int src, dst;

  hw_resource->occupy(node);

  vector<bool> involved_dim;
  for (int i = 0; i < node->getChakraNode()->involved_dim_size(); i++) {
    involved_dim.push_back(node->getChakraNode()->involved_dim(i));
  }

  if (node->getChakraNode()->node_type() == ChakraNodeType::COMM_COLL_NODE) {
    if (node->getChakraNode()->comm_type() == ChakraCollectiveCommType::ALL_REDUCE) {
      DataSet *fp = sys->generate_all_reduce(
          node->getChakraNode()->comm_size(),
          involved_dim,
          get_comm_group(1),
          node->getChakraNode()->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->getChakraNode()->id();
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (node->getChakraNode()->comm_type() == ChakraCollectiveCommType::ALL_TO_ALL) {
      DataSet *fp = sys->generate_all_to_all(
          node->getChakraNode()->comm_size(),
          involved_dim,
          get_comm_group(1),
          node->getChakraNode()->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->getChakraNode()->id();
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (node->getChakraNode()->comm_type() == ChakraCollectiveCommType::ALL_GATHER) {
      DataSet *fp = sys->generate_all_gather(
          node->getChakraNode()->comm_size(),
          involved_dim,
          get_comm_group(1),
          node->getChakraNode()->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->getChakraNode()->id();
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (node->getChakraNode()->comm_type() == ChakraCollectiveCommType::REDUCE_SCATTER) {
      DataSet *fp = sys->generate_reduce_scatter(
          node->getChakraNode()->comm_size(),
          involved_dim,
          get_comm_group(1),
          node->getChakraNode()->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->getChakraNode()->id();
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    }
  } else if (node->getChakraNode()->node_type() == ChakraNodeType::COMM_SEND_NODE) {
    sim_request snd_req;
    snd_req.srcRank = node->getChakraNode()->comm_src();
    snd_req.dstRank = node->getChakraNode()->comm_dst();
    snd_req.reqType = UINT8;
    SendPacketEventHandlerData *sehd = new SendPacketEventHandlerData;
    sehd->callable = this;
    sehd->wlhd = new WorkloadLayerHandlerData;
    sehd->wlhd->node_id = node->getChakraNode()->id();
    sehd->event = EventType::PacketSent;
    sys->front_end_sim_send(
            0,
            Sys::dummy_data,
            node->getChakraNode()->comm_size(),
            UINT8,
            node->getChakraNode()->comm_dst(),
            node->getChakraNode()->comm_tag(),
            &snd_req,
            &Sys::handleEvent,
            sehd);
  } else if (node->getChakraNode()->node_type() == ChakraNodeType::COMM_RECV_NODE) {
    sim_request rcv_req;
    RecvPacketEventHandlerData *rcehd = new RecvPacketEventHandlerData;
    rcehd->wlhd = new WorkloadLayerHandlerData;
    rcehd->wlhd->node_id = node->getChakraNode()->id();
    rcehd->workload = this;
    rcehd->event = EventType::PacketReceived;
    sys->front_end_sim_recv(
            0,
            Sys::dummy_data,
            node->getChakraNode()->comm_size(),
            UINT8,
            node->getChakraNode()->comm_src(),
            node->getChakraNode()->comm_tag(),
            &rcv_req,
            &Sys::handleEvent,
            rcehd);
  }
}

void Workload::skip_invalid(shared_ptr<Chakra::EGFeederNode> node) {
  eg_feeder->freeChildrenNodes(node->getChakraNode()->id());
  eg_feeder->removeNode(node->getChakraNode()->id());
}

void Workload::call(EventType event, CallData* data) {
  if (is_finished) {
    return;
  }

  if (event == EventType::CollectiveCommunicationFinished) {
    IntData* int_data = (IntData*)data;
    uint64_t node_id = collective_comm_node_id_map[int_data->data];
    shared_ptr<Chakra::EGFeederNode> node = eg_feeder->lookupNode(node_id);

    if (sys->trace_enabled) {
      cout << "callback,sys->id=" << sys->id
        << ",tick=" << Sys::boostedTick()
        << ",node->id=" << node->getChakraNode()->id()
        << ",node->name=" << node->getChakraNode()->name() << endl;
    }

    hw_resource->release(node);

    eg_feeder->freeChildrenNodes(node_id);

    issue_dep_free_nodes();

    eg_feeder->removeNode(node_id);

  } else {
    if (data == nullptr) {
      issue_dep_free_nodes();
    } else {
      WorkloadLayerHandlerData *wlhd = (WorkloadLayerHandlerData *)data;
      shared_ptr<Chakra::EGFeederNode> node = eg_feeder->lookupNode(wlhd->node_id);

      if (sys->trace_enabled) {
        cout << "callback,sys->id=" << sys->id
          << ",tick=" << Sys::boostedTick()
          << ",node->id=" << node->getChakraNode()->id()
          << ",node->name=" << node->getChakraNode()->name() << endl;
      }

      hw_resource->release(node);

      eg_feeder->freeChildrenNodes(node->getChakraNode()->id());

      issue_dep_free_nodes();

      eg_feeder->removeNode(wlhd->node_id);
      delete wlhd;
    }
  }

  if (!eg_feeder->hasNodesToIssue()
      && (hw_resource->num_in_flight_comps == 0)
      && (hw_resource->num_in_flight_comms == 0)) {
    report();
    is_finished = true;
  }
}

void Workload::fire() {
  call(EventType::General, NULL);
}

void Workload::report() {
  Tick curr_tick = Sys::boostedTick();
  cout << "workload[" << sys->id << "] finished, " << curr_tick << " cycles" << endl;
}
bool Workload::file_exists(const std::string& name){
  ifstream f(name.c_str());
  return f.good();
}