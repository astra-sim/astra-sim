/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/workload/Workload.hh"

#include <json/json.hpp>
#include "astra-sim/system/IntData.hh"
#include "astra-sim/system/MemEventHandlerData.hh"
#include "astra-sim/system/RecvPacketEventHandlerData.hh"
#include "astra-sim/system/SendPacketEventHandlerData.hh"
#include "astra-sim/system/WorkloadLayerHandlerData.hh"

#include <stdlib.h>
#include <unistd.h>
#include <iostream>

using namespace std;
using namespace AstraSim;
using namespace Chakra;
using json = nlohmann::json;

typedef ChakraProtoMsg::NodeType ChakraNodeType;
typedef ChakraProtoMsg::CollectiveCommType ChakraCollectiveCommType;

// TODO: Create a common library.
void check_filename(string filename) {
  if (access(filename.c_str(), R_OK) < 0) {
    string error_msg;
    if (errno == ENOENT) {
      error_msg = "file: " + filename + " does not exist";
    } else if (errno == EACCES) {
      error_msg =
          "file: " + filename + " exists but is not readable";
    } else {
      error_msg =
          "Unknown file: " + filename + " access error";
    }
    cerr << error_msg << endl;
    exit(EXIT_FAILURE);
  }
}

Workload::Workload(Sys* sys, string workload_filename, string comm_group_filename) {
  this->comm_group = nullptr;
  // TODO: parametrize the number of available hardware resources
  this->hw_resource = new HardwareResource(1);
  this->sys = sys;
  this->is_finished = false;

  initialize_comm_group(comm_group_filename);

  int workload_id = 0;
  if (this->comm_group != nullptr) {
    // Comm group ID starts at 1. Workload ID starts at 0.
    workload_id = this->comm_group->get_id() - 1;
  }

  string et_filename;
  if (workload_filename.find(".json") == std::string::npos) {
    // Traditional case where only the et trace prefix is provided. For backward compatability.
    et_filename = workload_filename;
    if (workload_id != 0) {
      string error_msg = "multiple communicator groups defined with single-tenant workload";
      cerr << error_msg << endl;
      exit(EXIT_FAILURE);
    }
  } else {
    // Workload file is provided. Read through workload file to find the et prefix for this node. In multi-tenant scenarios, there should be multiple et prefixes
    check_filename(workload_filename);
    ifstream inFile;
    json j;
    inFile.open(workload_filename);
    inFile >> j;

    for (json::iterator it = j.begin(); it != j.end(); ++it) {
      // There is only one valid key to this file - therefore, this for loop will exit immediately
      if (it.key() != "execution_trace_prefixes") {
        string error_msg = "workload file: " + workload_filename + " has wrong json key";
        cerr << error_msg << endl;
        exit(EXIT_FAILURE);
      }

      int idx = 0;
      bool found_et_filename = false;
      // Iterate through the list of execution trace prefix
      for (auto et_filename_input : it.value()) {
        if (idx == workload_id) {
          et_filename = et_filename_input;
          found_et_filename = true;
          break;
        }
        idx++;
      }

      if (!found_et_filename) {
        cerr << "Could not find et_filename for node " << sys->id << " with workload " << workload_id << " in workload file " << workload_filename << endl;
        exit(EXIT_FAILURE);
      }
    }
  }
  string trace_filename = et_filename + "." + to_string(sys->id) + ".et";
  check_filename(trace_filename);
  this->et_feeder = new ETFeeder(trace_filename);
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
    int comm_group_id;

    for (auto id : it.value()) {
      if (id == sys->id) {
        comm_group_id = stoi(it.key());
        in_comm_group = true;
        break;
      }
    }

    if (in_comm_group) {
      std::vector<int> involved_NPUs;
      for (auto id : it.value()) {
        involved_NPUs.push_back(id);
      }
      comm_group = new CommunicatorGroup(comm_group_id, involved_NPUs, sys);
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
    et_feeder->pushBackIssuableNode(node->id());
    push_back_queue.pop();
  }
}

void Workload::issue(shared_ptr<Chakra::ETFeederNode> node) {
  if (sys->replay_only) {
      hw_resource->occupy(node);
      issue_replay(node);
  } else {
    if ((node->type() == ChakraNodeType::MEM_LOAD_NODE)
        || (node->type() == ChakraNodeType::MEM_STORE_NODE)) {
      if (sys->trace_enabled) {
        cout << "issue,sys->id=" << sys->id
          << ",tick=" << Sys::boostedTick()
          << ",node->id=" << node->id()
          << ",node->name=" << node->name() << endl;
      }
      issue_remote_mem(node);
    } else if (
        node->is_cpu_op()
        || (!node->is_cpu_op() && node->type() == ChakraNodeType::COMP_NODE)) {
      if ((node->runtime() == 0) &&
          (node->num_ops() == 0)) {
        skip_invalid(node);
      } else {
        if (sys->trace_enabled) {
          cout << "issue,sys->id=" << sys->id << ",tick=" << Sys::boostedTick()
               << ",node->id=" << node->id()
               << ",node->name=" << node->name() << endl;
        }
        issue_comp(node);
      }
    } else if (
        !node->is_cpu_op() &&
        (node->type() == ChakraNodeType::COMM_COLL_NODE
         || (node->type() == ChakraNodeType::COMM_SEND_NODE)
         || (node->type() == ChakraNodeType::COMM_RECV_NODE))) {
      if (sys->trace_enabled) {
        cout << "issue,sys->id=" << sys->id << ",tick=" << Sys::boostedTick()
             << ",node->id=" << node->id()
             << ",node->name=" << node->name() << endl;
      }
      issue_comm(node);
    } else if (
        node->type() == ChakraNodeType::INVALID_NODE) {
      skip_invalid(node);
    }
  }
}

void Workload::issue_replay(shared_ptr<Chakra::ETFeederNode> node) {
  WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
  wlhd->node_id = node->id();

  assert(node->runtime() != 0);
  sys->register_event(
      this,
      EventType::General,
      wlhd,
      // chakra runtimes are in microseconds and we should convert it into nanoseconds
      node->runtime() * 1000);
}

void Workload::issue_remote_mem(shared_ptr<Chakra::ETFeederNode> node) {
  hw_resource->occupy(node);

  WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
  wlhd->sys_id = sys->id;
  wlhd->workload = this;
  wlhd->node_id = node->id();
  sys->remote_mem->issue(node->tensor_size(), wlhd);
}

void Workload::issue_comp(shared_ptr<Chakra::ETFeederNode> node) {
  hw_resource->occupy(node);

  if (sys->roofline_enabled) {
    WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
    wlhd->node_id = node->id();

    double operational_intensity =
        static_cast<double>(node->num_ops()) /
        static_cast<double>(node->tensor_size());
    double perf = sys->roofline->get_perf(operational_intensity);
    double elapsed_time =
        static_cast<double>(node->num_ops()) / perf;
    uint64_t runtime = static_cast<uint64_t>(elapsed_time);
    sys->register_event(this, EventType::General, wlhd, runtime);
  } else {
    // advance this node forward the recorded "replayed" time specificed in the ET.
    issue_replay(node);
  }
}

void Workload::issue_comm(shared_ptr<Chakra::ETFeederNode> node) {
  hw_resource->occupy(node);

  vector<bool> involved_dim;
  for (int i = 0; i < node->involved_dim_size(); i++) {
    involved_dim.push_back(node->involved_dim(i));
  }

  if (!node->is_cpu_op()
      && (node->type() == ChakraNodeType::COMM_COLL_NODE)) {
    if (node->comm_type() ==
        ChakraCollectiveCommType::ALL_REDUCE) {
      DataSet* fp = sys->generate_all_reduce(
          node->comm_size(),
          involved_dim,
          comm_group,
          node->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (node->comm_type() ==
        ChakraCollectiveCommType::ALL_TO_ALL) {
      DataSet* fp = sys->generate_all_to_all(
          node->comm_size(),
          involved_dim,
          comm_group,
          node->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (node->comm_type() ==
        ChakraCollectiveCommType::ALL_GATHER) {
      DataSet* fp = sys->generate_all_gather(
          node->comm_size(),
          involved_dim,
          comm_group,
          node->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (node->comm_type() ==
        ChakraCollectiveCommType::REDUCE_SCATTER) {
      DataSet* fp = sys->generate_reduce_scatter(
          node->comm_size(),
          involved_dim,
          comm_group,
          node->comm_priority());
      collective_comm_node_id_map[fp->my_id] = node->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
    } else if (node->comm_type() ==
        ChakraCollectiveCommType::BROADCAST) {
      // broadcast colelctive has not been implemented in ASTRA-SIM yet.
      // So, we just use its real system mesurements
      assert(node->runtime() != 0);
      DataSet* fp = new DataSet(1);
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
      sys->register_event(
        fp,
        EventType::General,
        nullptr,
        // chakra runtimes are in microseconds and we should convert it into nanoseconds
        node->runtime() * 1000);
      }
  } else if (node->type() == ChakraNodeType::COMM_SEND_NODE) {
    sim_request snd_req;
    snd_req.srcRank = node->comm_src();
    snd_req.dstRank = node->comm_dst();
    snd_req.reqType = UINT8;
    SendPacketEventHandlerData* sehd = new SendPacketEventHandlerData;
    sehd->callable = this;
    sehd->wlhd = new WorkloadLayerHandlerData;
    sehd->wlhd->node_id = node->id();
    sehd->event = EventType::PacketSent;
    sys->front_end_sim_send(
        0,
        Sys::dummy_data,
        node->comm_size(),
        UINT8,
        node->comm_dst(),
        node->comm_tag(),
        &snd_req,
        &Sys::handleEvent,
        sehd);
  } else if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
    sim_request rcv_req;
    RecvPacketEventHandlerData* rcehd = new RecvPacketEventHandlerData;
    rcehd->wlhd = new WorkloadLayerHandlerData;
    rcehd->wlhd->node_id = node->id();
    rcehd->workload = this;
    rcehd->event = EventType::PacketReceived;
    sys->front_end_sim_recv(
        0,
        Sys::dummy_data,
        node->comm_size(),
        UINT8,
        node->comm_src(),
        node->comm_tag(),
        &rcv_req,
        &Sys::handleEvent,
        rcehd);
  } else {
    cerr << "Unknown communication node type" << endl;
    exit(EXIT_FAILURE);
  }
}

void Workload::skip_invalid(shared_ptr<Chakra::ETFeederNode> node) {
  et_feeder->freeChildrenNodes(node->id());
  et_feeder->removeNode(node->id());
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
      cout << "callback,sys->id=" << sys->id << ",tick=" << Sys::boostedTick()
           << ",node->id=" << node->id()
           << ",node->name=" << node->name() << endl;
    }

    hw_resource->release(node);

    et_feeder->freeChildrenNodes(node_id);

    issue_dep_free_nodes();

    et_feeder->removeNode(node_id);

    // The Dataset class provides statistics that should be used later to dump
    // more statistics in the workload layer
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
             << ",node->id=" << node->id()
             << ",node->name=" << node->name() << endl;
      }

      hw_resource->release(node);

      et_feeder->freeChildrenNodes(node->id());

      issue_dep_free_nodes();

      et_feeder->removeNode(wlhd->node_id);
      delete wlhd;
    }
  }

  if (!et_feeder->hasNodesToIssue() &&
      (hw_resource->num_in_flight_cpu_ops == 0) &&
      (hw_resource->num_in_flight_gpu_comp_ops == 0) &&
      (hw_resource->num_in_flight_gpu_comm_ops == 0)) {
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
