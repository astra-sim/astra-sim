/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/workload/Workload.hh"

#include <json/json.hpp>
#include "astra-sim/common/Logging.hh"
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

Workload::Workload(Sys* sys, string et_filename, string comm_group_filename) {
  string workload_filename = et_filename + "." + to_string(sys->id) + ".et";
  // Check if workload filename exists
  if (access(workload_filename.c_str(), R_OK) < 0) {
    string error_msg;
    if (errno == ENOENT) {
      error_msg = "workload file: " + workload_filename + " does not exist";
    } else if (errno == EACCES) {
      error_msg =
          "workload file: " + workload_filename + " exists but is not readable";
    } else {
      error_msg =
          "Unknown workload file: " + workload_filename + " access error";
    }
    Logger::getLogger("workload")->critical(error_msg);
    exit(EXIT_FAILURE);
  }
  this->et_feeder = new ETFeeder(workload_filename);
  // TODO: parametrize the number of available hardware resources
  this->hw_resource = new HardwareResource(1);
  this->sys = sys;
  initialize_comm_group(comm_group_filename);
  this->is_finished = false;
}

Workload::~Workload() {
  this->comm_groups.clear();
  if (this->et_feeder != nullptr)
    delete this->et_feeder;
  if (this->hw_resource != nullptr)
    delete this->hw_resource;
}

void Workload::initialize_comm_group(string comm_group_filename) {
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
      int group_id = std::stoi(it.key());
      this->comm_groups.emplace(
          group_id, CommunicatorGroup(group_id, involved_NPUs, sys));
      // Note: All NPUs should create comm group with identical ids if they want
      // to communicate with each other
    }
  }
}

void Workload::issue_dep_free_nodes() {
  std::queue<shared_ptr<Chakra::ETFeederNode>> push_back_queue;
  shared_ptr<Chakra::ETFeederNode> node = et_feeder->getNextIssuableNode();
  auto logger = Logger::getLogger("workload");
  while (node != nullptr) {
#ifdef ASTRASIM_TRACE_HARDWARE_RESOURCES
    logger->trace("try issue node.id={}", node->id());
#endif
    if (hw_resource->is_available(node)) {
#ifdef ASTRASIM_TRACE_HARDWARE_RESOURCES
      logger->trace("issue success node.id={}", node->id());
#endif
      issue(node);
    } else {
#ifdef ASTRASIM_TRACE_HARDWARE_RESOURCES
      logger->trace("issue fail node.id={}", node->id());
#endif
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
  constexpr bool STRICT_MODE = true;
  if (sys->trace_enabled) {
    Logger::getLogger("workload")
        ->debug(
            "issue,sys->id={},tick={},node->id={},node->name={},node_type={}",
            sys->id,
            Sys::boostedTick(),
            node->id(),
            node->name(),
            static_cast<uint64_t>(node->type()));
  }
  const auto& node_type = node->type();

  hw_resource->occupy(node);
  try {
    if (sys->replay_only) {
      issue_replay(node);
      return;
    }

    // else not replay_only
    if ((node_type == ChakraNodeType::MEM_LOAD_NODE) ||
        (node_type == ChakraNodeType::MEM_STORE_NODE)) {
      issue_remote_mem(node);
    } else if (node_type == ChakraNodeType::COMP_NODE) {
      if (node->is_cpu_op(true))
        issue_cpu_comp(node);
      else
        issue_gpu_comp(node);
    } else if (
        (node_type == ChakraNodeType::COMM_COLL_NODE) ||
        (node_type == ChakraNodeType::COMM_SEND_NODE) ||
        (node_type == ChakraNodeType::COMM_RECV_NODE)) {
      issue_comm(node);
    } else if (node_type == ChakraNodeType::INVALID_NODE) {
      skip_invalid(node);
    } else {
      Logger::getLogger("workload")
          ->critical(
              "Invalid node_type, node.id={}, node.type={}",
              node->id(),
              static_cast<uint64_t>(node->type()));
      assert(false);
    }
  } catch (const MissingAttrException& e) {
    if (STRICT_MODE)
      throw(e);
    Logger::getLogger("workload")
        ->warn("Invalid node issue with message \"{}\", skiped", e.what());
    skip_invalid(node);
  }
  return;
}

void Workload::issue_replay(shared_ptr<Chakra::ETFeederNode> node) {
  if (node->runtime() == 0)
    throw MissingAttrException(
        "\"Zero runtime\" from node.id=" + to_string(node->id()) +
        " required by \"issue_replay\"");

  WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
  wlhd->node_id = node->id();

  sys->register_event(
      this,
      EventType::General,
      wlhd,
      // chakra runtimes are in microseconds and we should convert it into
      // nanoseconds
      node->runtime() * 1000);
}

void Workload::issue_remote_mem(shared_ptr<Chakra::ETFeederNode> node) {
  if (!node->has_tensor_size())
    throw MissingAttrException("tensor_size", node->id(), "issue_remote_mem");

  WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
  wlhd->sys_id = sys->id;
  wlhd->workload = this;
  wlhd->node_id = node->id();
  sys->remote_mem->issue(node->tensor_size(), wlhd);
}

void Workload::issue_cpu_comp(shared_ptr<Chakra::ETFeederNode> node) {
  assert(node->is_cpu_op(true));
  if (!sys->roofline_enabled) {
    issue_replay(node);
    return;
  }

  if (!node->has_num_ops())
    throw MissingAttrException("num_ops", node->id(), "issue_cpu_comp");
  if (!node->has_tensor_size())
    throw MissingAttrException("tensor_size", node->id(), "issue_cpu_comp");

  WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
  wlhd->node_id = node->id();

  double operational_intensity = static_cast<double>(node->num_ops()) /
      static_cast<double>(node->tensor_size());
  double perf = sys->roofline->get_perf(operational_intensity);
  double elapsed_time = static_cast<double>(node->num_ops()) / perf;
  uint64_t runtime = static_cast<uint64_t>(elapsed_time);
  sys->register_event(this, EventType::General, wlhd, runtime);
}

void Workload::issue_gpu_comp(shared_ptr<Chakra::ETFeederNode> node) {
  assert(!node->is_cpu_op(true));
  if (!sys->roofline_enabled) {
    issue_replay(node);
    return;
  }
  throw std::runtime_error(
      "Do not support non-replay gpu comp nodes yet, need to add another roofline model for gpu");
}

void Workload::issue_comm(shared_ptr<Chakra::ETFeederNode> node) {
  if (!node->has_comm_size())
    throw MissingAttrException("comm_size", node->id(), "issue_comm");
  if (node->type() == ChakraNodeType::COMM_COLL_NODE) {
    if (!node->has_comm_type())
      throw MissingAttrException("comm_type", node->id(), "issue_comm");
  } else if (node->type() == ChakraNodeType::COMM_SEND_NODE) {
    if (node->comm_src(sys->id) != sys->id) {
      Logger::getLogger("workload")
          ->critical(
              "sys issue a send comm node {} with src{} != sys.id{}",
              node->id(),
              node->comm_src(),
              sys->id);
      exit(EXIT_FAILURE);
    }
    if (!node->has_comm_dst())
      throw MissingAttrException("comm_dst", node->id(), "issue_comm");
    if (!node->has_comm_tag())
      throw MissingAttrException("comm_tag", node->id(), "issue_comm");
  } else if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
    if (!node->has_comm_src())
      throw MissingAttrException("comm_src", node->id(), "issue_comm");
    if (!node->has_comm_tag())
      throw MissingAttrException("comm_tag", node->id(), "issue_comm");
  } else {
    Logger::getLogger("workload")->critical("Unknown communication node type");
    exit(EXIT_FAILURE);
  }

  const uint32_t comm_priority = node->comm_priority(0u);

  vector<bool> involved_dim;
  if (node->has_involved_dim_size()) {
    // TODO: get topology dimension from network instead of
    // all_reduce_implementation
    if (node->involved_dim_size() !=
        this->sys->all_reduce_implementation_per_dimension.size()) {
      Logger::getLogger("workload")
          ->critical(
              "node.id={}. involved_dim.size unmatched to number of topology dimensions",
              node->id());
      exit(EXIT_FAILURE);
    }
    for (int i = 0; i < node->involved_dim_size(); i++) {
      involved_dim.push_back(node->involved_dim(i));
    }
  } else {
    // by default all dimension involved
    for (int i = 0;
         i < this->sys->all_reduce_implementation_per_dimension.size();
         i++) {
      involved_dim.push_back(true);
    }
  }

  CommunicatorGroup* comm_group = nullptr;
  if (node->has_other_attr("comm_group")) {
    int comm_group_id = node->get_other_attr("comm_group").int32_val();
    auto it = this->comm_groups.find(comm_group_id);
    if (it == this->comm_groups.end()) {
      Logger::getLogger("workload")
          ->critical(
              "node.id={}. Request comm_group.id={}, which is not defined in sys.id={}",
              node->id(),
              comm_group_id,
              this->sys->id);
      exit(EXIT_FAILURE);
    }
    comm_group = &(it->second);
  }

  if (node->type() == ChakraNodeType::COMM_COLL_NODE) {
    if (node->comm_type() == ChakraCollectiveCommType::ALL_REDUCE) {
      DataSet* fp = sys->generate_all_reduce(
          node->comm_size(), involved_dim, comm_group, comm_priority);
      collective_comm_node_id_map[fp->my_id] = node->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
    } else if (node->comm_type() == ChakraCollectiveCommType::ALL_TO_ALL) {
      DataSet* fp = sys->generate_all_to_all(
          node->comm_size(), involved_dim, comm_group, comm_priority);
      collective_comm_node_id_map[fp->my_id] = node->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (node->comm_type() == ChakraCollectiveCommType::ALL_GATHER) {
      DataSet* fp = sys->generate_all_gather(
          node->comm_size(), involved_dim, comm_group, comm_priority);
      collective_comm_node_id_map[fp->my_id] = node->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

    } else if (node->comm_type() == ChakraCollectiveCommType::REDUCE_SCATTER) {
      DataSet* fp = sys->generate_reduce_scatter(
          node->comm_size(), involved_dim, comm_group, comm_priority);
      collective_comm_node_id_map[fp->my_id] = node->id();
      collective_comm_wrapper_map[fp->my_id] = fp;
      fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
    } else if (node->comm_type() == ChakraCollectiveCommType::BROADCAST) {
      // broadcast colelctive has not been implemented in ASTRA-SIM yet.
      // So, we just use its real system mesurements
      issue_replay(node);
    }
  } else if (node->type() == ChakraNodeType::COMM_SEND_NODE) {
    sim_request snd_req;
    snd_req.srcRank = sys->id;
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
    assert(false);
  }
}

void Workload::skip_invalid(shared_ptr<Chakra::ETFeederNode> node) {
  et_feeder->freeChildrenNodes(node->id());
  et_feeder->removeNode(node->id());
  hw_resource->release(node);
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
      Logger::getLogger("workload")
          ->debug(
              "callback,sys->id={},tick={},node->id={},node->name={},node_type={}",
              sys->id,
              Sys::boostedTick(),
              node->id(),
              node->name(),
              static_cast<uint64_t>(node->type()));
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
        Logger::getLogger("workload")
            ->debug(
                "callback,sys->id={},tick={},node->id={},node->name={},node_type={}",
                sys->id,
                Sys::boostedTick(),
                node->id(),
                node->name(),
                static_cast<uint64_t>(node->type()));
      }

      hw_resource->release(node);

      et_feeder->freeChildrenNodes(node->id());

      issue_dep_free_nodes();

      et_feeder->removeNode(wlhd->node_id);
      delete wlhd;
    }
  }

  if (!et_feeder->hasNodesToIssue() &&
      (hw_resource->num_in_flight_cpu_comp_ops == 0) &&
      (hw_resource->num_in_flight_gpu_comp_ops == 0) &&
      (hw_resource->num_in_flight_comm_ops == 0) &&
      (hw_resource->num_in_flight_mem_ops == 0)) {
    report();
    is_finished = true;
  }
}

void Workload::fire() {
  call(EventType::General, NULL);
}

void Workload::report() {
  Tick curr_tick = Sys::boostedTick();
  Logger::getLogger("workload")
      ->info("sys[{}] finished, {} cycles", sys->id, curr_tick);
}
