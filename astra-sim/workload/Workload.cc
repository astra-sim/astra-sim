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

#include <iostream>
#include <unistd.h>
#include <stdlib.h> 

using namespace std;
using namespace AstraSim;
using namespace Chakra;
using json = nlohmann::json;

typedef ChakraProtoMsg::NodeType ChakraNodeType;
typedef ChakraProtoMsg::CollectiveCommType ChakraCollectiveCommType;

Workload::Workload(Sys* sys, string eg_filename, string comm_group_filename) {
  string workload_filename=eg_filename + "." + to_string(sys->id) + ".et";
  //Checking if workload filename exist
  int res = access(workload_filename.c_str(), R_OK);
  if (res < 0) {
	if (errno == ENOENT) {
		// file does not exist
		cerr<<"workload file: "<< workload_filename<<" does not exist"<<endl;
		exit (EXIT_FAILURE);
		
	} else if (errno == EACCES) {
		// file exists but is not readable
		cerr<<"workload file: "<< workload_filename<<" exists but is not readble"<<endl;;
		exit (EXIT_FAILURE);
	} else {
		 // uh oh
		cerr<<"Unknown workload file: "<<workload_filename<<" access error"<<endl;;
		exit (EXIT_FAILURE);
	}
  }
  this->et_feeder =
      new ETFeeder(workload_filename);
  this->comm_group = nullptr;
  // TODO: parametrize the number of available hardware resources
  this->hw_resource = new HardwareResource(1);
  this->sys = sys;
  initialize_comm_group(comm_group_filename);
  this->is_finished = false;
}

Workload::~Workload() {
  if (this->comm_group != nullptr)
    delete this->comm_group;
  if (this->et_feeder != nullptr)
    delete this->et_feeder;
}

void Workload::initialize_comm_group(string comm_group_filename) {
  // communicator group input file is not given
  if (comm_group_filename.find("empty") != std::string::npos) {
    comm_group = nullptr;
    return;
  }

  ifstream inFile;
  json j;
  inFile.open(comm_group_filename);
  inFile >> j;

  for (json::iterator it = j.begin(); it != j.end(); ++it) {
    bool in_comm_group = false;

    for (auto id : it.value()) {
      if (id == sys->id) {
        in_comm_group = true;
      }
    }

    if (in_comm_group) {
      std::vector<int> involved_NPUs;
      for (auto id : it.value()) {
        involved_NPUs.push_back(id);
      }
      comm_group = new CommunicatorGroup(1, involved_NPUs, sys);
      // Note: All NPUs should create comm group with identical ids if they want
      // to communicate with each other
    }
  }
}

void Workload::issue_dep_free_nodes() {
  std::queue<shared_ptr<Chakra::ETFeederNode>> push_back_queue;
  shared_ptr<Chakra::ETFeederNode> node = et_feeder->getNextIssuableNode();
  
  while (node != nullptr) {
    if (hw_resource->is_available(node)) {
      issue(node);
    } else {
      push_back_queue.push(node);
    }
    node = et_feeder->getNextIssuableNode();
  }

  while (!push_back_queue.empty()) {
    shared_ptr<Chakra::ETFeederNode> node = push_back_queue.front();
    et_feeder->pushBackIssuableNode(node->getChakraNode()->id());
    push_back_queue.pop();
  }
  
}

void Workload::issue(shared_ptr<Chakra::ETFeederNode> node) {
  if (
      sys->replay_only ||
      (node->getChakraNode()->type() == ChakraNodeType::COMP_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_COLL_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_SEND_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_RECV_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMP_NODE)) {
      if ((node->getChakraNodeFlattened()->runtime() == 0) &&
          (node->getChakraNodeFlattened()->num_ops() == 0)) {
      skip_invalid(node);
      } else {
        if (sys->trace_enabled) {
          cout << "issue comp node,sys->id=" << sys->id << ",tick=" << Sys::boostedTick()
              << ",node->id=" << node->getChakraNode()->id()
              << ",node->name=" << node->getChakraNode()->name().substr(0,20) <<", dur: "<<node->getChakraNodeFlattened()->runtime()*1000<<" ns"<< endl;
        }
        issue_comp(node);
      }
  } else if (
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_COLL_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_SEND_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_RECV_NODE)) {
    if (sys->trace_enabled) {
      cout << "issue comm node,sys->id=" << sys->id << ",tick=" << Sys::boostedTick()
           << ",node->id=" << node->getChakraNode()->id()
           << ",node->name=" << node->getChakraNode()->name().substr(0,20)<<", comm size: "<<node->getChakraNodeFlattened()->comm_size() << endl;
    }
    issue_comm(node);
  } else if (
      node->getChakraNode()->type() == ChakraNodeType::INVALID_NODE) {
    skip_invalid(node);
  }
}

void Workload::issue_comp(shared_ptr<Chakra::ETFeederNode> node) {
  //assert(node->getChakraNode()->type() == ChakraNodeType::COMP_NODE);
  hw_resource->occupy(node);

  if (sys->roofline_enabled) {
    WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
    wlhd->node_id = node->getChakraNode()->id();

    double operational_intensity =
        static_cast<double>(node->getChakraNodeFlattened()->num_ops()) /
        static_cast<double>(node->getChakraNodeFlattened()->tensor_size());
    double perf = sys->roofline->get_perf(operational_intensity);
    double elapsed_time =
        static_cast<double>(node->getChakraNodeFlattened()->num_ops()) / perf;
    uint64_t runtime =
        static_cast<uint64_t>(elapsed_time * static_cast<double>(FREQ));
    sys->register_event(this, EventType::General, wlhd, runtime);
  } else {
    WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
    wlhd->node_id = node->getChakraNode()->id();

    sys->register_event(
        this,
        EventType::General,
        wlhd,
        //chakra runtimes are in microseconds and we should convert it into nanoseconds
        node->getChakraNodeFlattened()->runtime()*1000); 
  }
}

void Workload::issue_comm(shared_ptr<Chakra::ETFeederNode> node) {
  int src, dst;
  //node->getChakraNodeFlattened()->print_node_type();
  //node->getChakraNodeFlattened()->print_comm_coll_type();
  assert(node->getChakraNodeFlattened()->comm_size()>0);
  hw_resource->occupy(node);

  vector<bool> involved_dim;
  for (int i = 0; i < node->getChakraNodeFlattened()->involved_dim_size(); i++) {
    involved_dim.push_back(node->getChakraNodeFlattened()->involved_dim(i));
  }

  if (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_COLL_NODE) {
    assert(involved_dim.size()>0);
    
    /*if(sys->trace_enabled){
      //node->getChakraNodeFlattened()->print_comm_coll_type();
      cout << "collective comm issue,sys->id=" << sys->id << ",tick=" << Sys::boostedTick()
            << ",node->id=" << node->getChakraNode()->id()
            << ",node->name=" << node->getChakraNode()->name()<<", comm size: "<<node->getChakraNodeFlattened()->comm_size()<< endl;
    }*/
    if (node->getChakraNodeFlattened()->comm_type() ==
        ChakraCollectiveCommType::ALL_REDUCE) {
      DataSet* fp = sys->generate_all_reduce(
          node->getChakraNodeFlattened()->comm_size(),
          involved_dim,
          comm_group,
          node->getChakraNodeFlattened()->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->getChakraNode()->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (
        node->getChakraNodeFlattened()->comm_type() ==
        ChakraCollectiveCommType::ALL_TO_ALL) {
      DataSet* fp = sys->generate_all_to_all(
          node->getChakraNodeFlattened()->comm_size(),
          involved_dim,
          comm_group,
          node->getChakraNodeFlattened()->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->getChakraNode()->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (
        node->getChakraNodeFlattened()->comm_type() ==
        ChakraCollectiveCommType::ALL_GATHER) {
      DataSet* fp = sys->generate_all_gather(
          node->getChakraNodeFlattened()->comm_size(),
          involved_dim,
          comm_group,
          node->getChakraNodeFlattened()->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->getChakraNode()->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (
        node->getChakraNodeFlattened()->comm_type() ==
        ChakraCollectiveCommType::REDUCE_SCATTER) {
      DataSet* fp = sys->generate_reduce_scatter(
          node->getChakraNodeFlattened()->comm_size(),
          involved_dim,
          comm_group,
          node->getChakraNodeFlattened()->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->getChakraNode()->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
    } else if (
        node->getChakraNodeFlattened()->comm_type() ==
        ChakraCollectiveCommType::BROADCAST) {
      //broadcast colelctive has not been implemented in ASTRA-SIM yet. So, we just use its real system mesurements
      DataSet* fp = new DataSet(1);
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
      sys->register_event(
        fp,
        EventType::General,
        nullptr,
        //chakra runtimes are in microseconds and we should convert it into nanoseconds
        node->getChakraNodeFlattened()->runtime()*1000);
      }
  } 
  else if (
    node->getChakraNode()->type() == ChakraNodeType::COMM_SEND_NODE) {
    sim_request snd_req;
    snd_req.srcRank = node->getChakraNodeFlattened()->comm_src();
    snd_req.dstRank = node->getChakraNodeFlattened()->comm_dst();
    snd_req.reqType = UINT8;
    SendPacketEventHandlerData* sehd = new SendPacketEventHandlerData;
    sehd->callable = this;
    sehd->wlhd = new WorkloadLayerHandlerData;
    sehd->wlhd->node_id = node->getChakraNode()->id();
    sehd->event = EventType::PacketSent;
    sys->front_end_sim_send(
        0,
        Sys::dummy_data,
        node->getChakraNodeFlattened()->comm_size(),
        UINT8,
        node->getChakraNodeFlattened()->comm_dst(),
        node->getChakraNodeFlattened()->comm_tag(),
        &snd_req,
        &Sys::handleEvent,
        sehd);
  } else if (
      node->getChakraNode()->type() == ChakraNodeType::COMM_RECV_NODE) {
    sim_request rcv_req;
    RecvPacketEventHandlerData* rcehd = new RecvPacketEventHandlerData;
    rcehd->wlhd = new WorkloadLayerHandlerData;
    rcehd->wlhd->node_id = node->getChakraNode()->id();
    rcehd->workload = this;
    rcehd->event = EventType::PacketReceived;
    sys->front_end_sim_recv(
        0,
        Sys::dummy_data,
        node->getChakraNodeFlattened()->comm_size(),
        UINT8,
        node->getChakraNodeFlattened()->comm_src(),
        node->getChakraNodeFlattened()->comm_tag(),
        &rcv_req,
        &Sys::handleEvent,
        rcehd);
  } else{
    cerr<<"Unknown comm node type"<<endl;
    exit (EXIT_FAILURE);
  }
}

void Workload::skip_invalid(shared_ptr<Chakra::ETFeederNode> node) {
  et_feeder->freeChildrenNodes(node->getChakraNode()->id());
  et_feeder->removeNode(node->getChakraNode()->id());
}

void Workload::call(EventType event, CallData* data) {
  if (is_finished) {
    return;
  }

  if (event == EventType::CollectiveCommunicationFinished) {
    IntData* int_data = (IntData*)data;
    uint64_t node_id = collective_comm_node_id_map[int_data->data];
    shared_ptr<Chakra::ETFeederNode> node = et_feeder->lookupNode(node_id);

    if (sys->trace_enabled) {
      cout << "collective comm callback,sys->id=" << sys->id << ",tick=" << Sys::boostedTick()
           << ",node->id=" << node->getChakraNode()->id()
           << ",node->name=" << node->getChakraNode()->name().substr(0,20)<<", comm size: "<<node->getChakraNodeFlattened()->comm_size()<< endl;
    }

    hw_resource->release(node);

    et_feeder->freeChildrenNodes(node_id);

    issue_dep_free_nodes();

    et_feeder->removeNode(node_id);
    // The Dataset class provides statistics that should be used later to dump more stats in the workload layer
    delete collective_comm_wrapper_map[node_id];
    collective_comm_wrapper_map.erase(node_id);

  } else {
    if (data == nullptr) {
      issue_dep_free_nodes();
    } else {
      WorkloadLayerHandlerData* wlhd = (WorkloadLayerHandlerData*)data;
      shared_ptr<Chakra::ETFeederNode> node =
          et_feeder->lookupNode(wlhd->node_id);
      if (sys->trace_enabled) {
        cout << "callback,sys->id=" << sys->id << ",tick=" << Sys::boostedTick()
             << ",node->id=" << node->getChakraNode()->id()
             << ",node->name=" << node->getChakraNode()->name().substr(0,20)<< " with simulated runtime of: "
             <<node->getChakraNodeFlattened()->runtime()*1000<<" ns"<< endl;
      }
      hw_resource->release(node);

      et_feeder->freeChildrenNodes(node->getChakraNode()->id());

      issue_dep_free_nodes();

      et_feeder->removeNode(wlhd->node_id);
      delete wlhd;
    }
  }
  /*if(sys->id==3)
    cout<<"for npu: "<<sys->id<<" hasNodesToIssue: "<<et_feeder->hasNodesToIssue()<<", num in-flight-comps: "<<hw_resource->num_in_flight_comps<<", num in-fligh-comms: "
    <<hw_resource->num_in_flight_comms<<", dep_graph size: "<<et_feeder->dep_graph_.size()<<", dep_free_nodes size: "<<et_feeder->dep_free_node_queue_.size()<<
    " and cycle check is: "<<et_feeder->check_dep_graph_cycle()<<endl;*/
  /*if(et_feeder->dep_graph_.size()==4)
    for(auto &a:et_feeder->dep_graph_){
      //a.second->getChakraNodeFlattened()->print_node_type();
      //a.second->getChakraNodeFlattened()->print_comm_coll_type();
      cout<<"And comm size is: "<<a.second->getChakraNodeFlattened()->comm_size()<<" and comm tag is: "<<a.second->getChakraNodeFlattened()->comm_tag()
      <<" and hw available is: "<<hw_resource->is_available(a.second)<<endl;
    }*/
  if (!et_feeder->hasNodesToIssue() &&
      (hw_resource->num_in_flight_host_ops == 0) &&
      (hw_resource->num_in_flight_kernel_ops == 0)) {
    report();
    is_finished = true;
  }
}

void Workload::fire() {
  call(EventType::General, NULL);
}

void Workload::report() {
  Tick curr_tick = Sys::boostedTick();
  cout << "sys[" << sys->id << "] finished, " << curr_tick << " cycles" << endl;
}
