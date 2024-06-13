/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/Sys.hh"

#include <cstdlib>
#include <iostream>

#include <json/json.hpp>
#include "astra-sim/common/Logging.hh"
#include "astra-sim/system/BaseStream.hh"
#include "astra-sim/system/CollectivePlan.hh"
#include "astra-sim/system/DataSet.hh"
#include "astra-sim/system/MemBus.hh"
#include "astra-sim/system/MemEventHandlerData.hh"
#include "astra-sim/system/QueueLevels.hh"
#include "astra-sim/system/RendezvousRecvData.hh"
#include "astra-sim/system/RendezvousSendData.hh"
#include "astra-sim/system/SendPacketEventHandlerData.hh"
#include "astra-sim/system/SimRecvCaller.hh"
#include "astra-sim/system/SimSendCaller.hh"
#include "astra-sim/system/StreamBaseline.hh"
#include "astra-sim/system/WorkloadLayerHandlerData.hh"
#include "astra-sim/system/collective/AllToAll.hh"
#include "astra-sim/system/collective/DoubleBinaryTreeAllReduce.hh"
#include "astra-sim/system/collective/HalvingDoubling.hh"
#include "astra-sim/system/collective/Ring.hh"
#include "astra-sim/system/scheduling/OfflineGreedy.hh"
#include "astra-sim/system/topology/BasicLogicalTopology.hh"
#include "astra-sim/system/topology/GeneralComplexTopology.hh"

using namespace std;
using namespace Chakra;
using json = nlohmann::json;

namespace AstraSim {
uint8_t* Sys::dummy_data = new uint8_t[2];
vector<Sys*> Sys::all_sys;

// SchedulerUnit --------------------------------------------------------------
Sys::SchedulerUnit::SchedulerUnit(
    Sys* sys,
    vector<int> queues,
    int max_running_streams,
    int ready_list_threshold,
    int queue_threshold) {
  this->sys = sys;
  this->max_running_streams = max_running_streams;
  this->ready_list_threshold = ready_list_threshold;
  this->queue_threshold = queue_threshold;
  this->latency_per_dimension.resize(queues.size(), 0);
  this->total_chunks_per_dimension.resize(queues.size(), 0);
  this->total_active_chunks_per_dimension.resize(queues.size(), 0);

  int base = 0;
  int dimension = 0;
  for (auto q : queues) {
    for (int i = 0; i < q; i++) {
      this->running_streams[base] = 0;
      list<BaseStream*>::iterator it;
      this->stream_pointer[base] = it;
      this->queue_id_to_dimension[base] = dimension;
      base++;
    }
    dimension++;
    UsageTracker u(2);
    usage.push_back(u);
  }
}

void Sys::SchedulerUnit::notify_stream_added(int vnet) {
  if (sys->id == 0 &&
      ++total_active_chunks_per_dimension[queue_id_to_dimension[vnet]] == 1) {
    usage[queue_id_to_dimension[vnet]].increase_usage();
  }
  stream_pointer[vnet] = sys->active_Streams[vnet].begin();
  advance(stream_pointer[vnet], running_streams[vnet]);
  while (stream_pointer[vnet] != sys->active_Streams[vnet].end() &&
         running_streams[vnet] < queue_threshold) {
    (*stream_pointer[vnet])->init();
    running_streams[vnet]++;
    advance(stream_pointer[vnet], 1);
  }
}

void Sys::SchedulerUnit::notify_stream_added_into_ready_list() {
  if (this->sys->first_phase_streams < ready_list_threshold &&
      this->sys->total_running_streams < max_running_streams) {
    int max = ready_list_threshold - sys->first_phase_streams;
    if (max > max_running_streams - this->sys->total_running_streams) {
      max = max_running_streams - this->sys->total_running_streams;
    }
    sys->schedule(max);
  }
  return;
}

void Sys::SchedulerUnit::notify_stream_removed(int vnet, Tick running_time) {
  if (sys->id == 0 &&
      --total_active_chunks_per_dimension[queue_id_to_dimension[vnet]] == 0) {
    usage[queue_id_to_dimension[vnet]].decrease_usage();
  }
  running_streams[vnet]--;

  int dimension = this->queue_id_to_dimension[vnet];
  latency_per_dimension[dimension] += running_time;
  total_chunks_per_dimension[dimension]++;

  if (this->sys->first_phase_streams < ready_list_threshold &&
      this->sys->total_running_streams < max_running_streams) {
    int max = ready_list_threshold - sys->first_phase_streams;
    if (max > max_running_streams - this->sys->total_running_streams) {
      max = max_running_streams - this->sys->total_running_streams;
    }
    sys->schedule(max);
  }
  stream_pointer[vnet] = sys->active_Streams[vnet].begin();
  advance(stream_pointer[vnet], running_streams[vnet]);
  while (stream_pointer[vnet] != sys->active_Streams[vnet].end() &&
         running_streams[vnet] < queue_threshold) {
    (*stream_pointer[vnet])->init();
    running_streams[vnet]++;
    advance(stream_pointer[vnet], 1);
  }
}

vector<double> Sys::SchedulerUnit::get_average_latency_per_dimension() {
  vector<double> result;
  result.resize(latency_per_dimension.size(), -1);
  for (uint64_t i = 0; i < result.size(); i++) {
    result[i] = latency_per_dimension[i] / total_chunks_per_dimension[i];
  }
  return result;
}
//-----------------------------------------------------------------------------

Sys::Sys(
    int id,
    string workload_configuration,
    string comm_group_configuration,
    string system_configuration,
    AstraRemoteMemoryAPI* remote_mem,
    AstraNetworkAPI* comm_NI,
    vector<int> physical_dims,
    vector<int> queues_per_dim,
    double injection_scale,
    double comm_scale,
    bool rendezvous_enabled) {
  if ((id + 1) > this->all_sys.size()) {
    this->all_sys.resize(id + 1);
  }
  this->all_sys[id] = this;

  this->id = id;
  this->initialized = false;

  this->workload = nullptr;

  this->roofline_enabled = false;
  this->peak_perf = 0;
  this->roofline = nullptr;

  this->remote_mem = remote_mem;
  this->remote_mem->set_sys(id, this);
  this->local_mem_bw = 0;

  this->memBus = nullptr;
  this->inp_L = 0;
  this->inp_o = 0;
  this->inp_g = 0;
  this->inp_G = 0;
  this->model_shared_bus = 0;
  this->injection_scale = injection_scale;
  this->communication_delay = 0;
  this->local_reduction_delay = 0;

  this->comm_NI = comm_NI;
  this->comm_scale = comm_scale;
  this->rendezvous_enabled = rendezvous_enabled;

  this->scheduler_unit = nullptr;
  this->vLevels = nullptr;
  this->offline_greedy = nullptr;
  this->intra_dimension_scheduling = IntraDimensionScheduling::FIFO;
  this->inter_dimension_scheduling = InterDimensionScheduling::Ascending;
  this->round_robin_inter_dimension_scheduler = 0;
  this->active_chunks_per_dimension = 1;
  this->priority_counter = 0;
  this->pending_events = 0;
  this->preferred_dataset_splits = 0;

  this->last_scheduled_collective = 0;

  this->first_phase_streams = 0;
  this->total_running_streams = 0;

  this->communication_delay = 10;
  this->local_reduction_delay = 1;

  if (initialize_sys(system_configuration) == false) {
    sys_panic(
        "Unable to initialize the system layer because the file can not be openned");
  }

  // scheduler
  this->physical_dims = physical_dims;
  this->queues_per_dim = queues_per_dim;
  int element = 0;
  this->total_nodes = 1;
  this->dim_to_break = -1;
  for (uint64_t current_dim = 0; current_dim < queues_per_dim.size();
       current_dim++) {
    if (physical_dims[current_dim] >= 1) {
      this->total_nodes *= physical_dims[current_dim];
    }
    for (int j = 0; j < queues_per_dim[current_dim]; j++) {
      list<BaseStream*> temp;
      active_Streams[element] = temp;
      list<int> pri;
      stream_priorities[element] = pri;
      element++;
    }
  }

  this->concurrent_streams =
      (int)ceil(((double)active_chunks_per_dimension) / queues_per_dim[0]);
  this->active_first_phase = 100000000;
  this->max_running = 100000000;

  scheduler_unit = new SchedulerUnit(
      this,
      queues_per_dim,
      max_running,
      active_first_phase,
      concurrent_streams);

  vLevels = new QueueLevels(queues_per_dim, 0, comm_NI->get_backend_type());

  // collective communication
  this->num_streams = 0;

  logical_topologies["AllReduce"] = new GeneralComplexTopology(
      id, physical_dims, all_reduce_implementation_per_dimension);
  logical_topologies["ReduceScatter"] = new GeneralComplexTopology(
      id, physical_dims, reduce_scatter_implementation_per_dimension);
  logical_topologies["AllGather"] = new GeneralComplexTopology(
      id, physical_dims, all_gather_implementation_per_dimension);
  logical_topologies["AllToAll"] = new GeneralComplexTopology(
      id, physical_dims, all_to_all_implementation_per_dimension);

  memBus = new MemBus(
      "NPU",
      "MA",
      this,
      inp_L,
      inp_o,
      inp_g,
      inp_G,
      model_shared_bus,
      communication_delay,
      true);

  workload =
      new Workload(this, workload_configuration, comm_group_configuration);

  if (inter_dimension_scheduling == InterDimensionScheduling::OfflineGreedy ||
      inter_dimension_scheduling ==
          InterDimensionScheduling::OfflineGreedyFlex) {
    offline_greedy = new OfflineGreedy(this);
  }

  this->break_dimension_done = false;
  this->dimension_to_break = 0;

  this->initialized = true;
}

Sys::~Sys() {
  if (roofline_enabled) {
    delete this->roofline;
  }

  all_sys[id] = nullptr;

  for (auto lt : logical_topologies) {
    delete lt.second;
  }

  logical_topologies.clear();

  for (auto ci : all_reduce_implementation_per_dimension) {
    delete ci;
  }
  for (auto ci : reduce_scatter_implementation_per_dimension) {
    delete ci;
  }
  for (auto ci : all_gather_implementation_per_dimension) {
    delete ci;
  }
  for (auto ci : all_to_all_implementation_per_dimension) {
    delete ci;
  }

  if (scheduler_unit != nullptr)
    delete scheduler_unit;

  if (vLevels != nullptr)
    delete vLevels;

  if (memBus != nullptr)
    delete memBus;

  if (workload != nullptr)
    delete workload;

  if (offline_greedy != nullptr)
    delete offline_greedy;

  bool shouldExit = true;
  for (auto& a : all_sys) {
    if (a != nullptr) {
      shouldExit = false;
      break;
    }
  }

  if (shouldExit) {
    exit_sim_loop("Exiting");
  }
}

bool Sys::initialize_sys(string name) {
  ifstream inFile;
  inFile.open(name);
  if (!inFile) {
    if (id == 0) {
      LoggerFactory::get_logger("system")->critical(
          "Unable to open file: {}", name);
    }
    exit(1);
  }

  json j;
  inFile >> j;
  if (j.contains("scheduling-policy")) {
    string inp_scheduling_policy = j["scheduling-policy"];
    if (inp_scheduling_policy == "LIFO") {
      this->scheduling_policy = SchedulingPolicy::LIFO;
    } else if (inp_scheduling_policy == "FIFO") {
      this->scheduling_policy = SchedulingPolicy::FIFO;
    } else if (inp_scheduling_policy == "EXPLICIT") {
      this->scheduling_policy = SchedulingPolicy::EXPLICIT;
    } else {
      sys_panic("unknown value for scheduling policy in sys input file");
    }
  }
  if (j.contains("all-reduce-implementation")) {
    vector<string> collective_impl_str_vec = j["all-reduce-implementation"];
    for (auto collective_impl_str : collective_impl_str_vec) {
      CollectiveImpl* ci =
          generate_collective_impl_from_input(collective_impl_str);
      all_reduce_implementation_per_dimension.push_back(ci);
    }
  }
  if (j.contains("reduce-scatter-implementation")) {
    vector<string> collective_impl_str_vec = j["reduce-scatter-implementation"];
    for (auto collective_impl_str : collective_impl_str_vec) {
      CollectiveImpl* ci =
          generate_collective_impl_from_input(collective_impl_str);
      reduce_scatter_implementation_per_dimension.push_back(ci);
    }
  }
  if (j.contains("all-gather-implementation")) {
    vector<string> collective_impl_str_vec = j["all-gather-implementation"];
    for (auto collective_impl_str : collective_impl_str_vec) {
      CollectiveImpl* ci =
          generate_collective_impl_from_input(collective_impl_str);
      all_gather_implementation_per_dimension.push_back(ci);
    }
  }
  if (j.contains("all-to-all-implementation")) {
    vector<string> collective_impl_str_vec = j["all-to-all-implementation"];
    for (auto collective_impl_str : collective_impl_str_vec) {
      CollectiveImpl* ci =
          generate_collective_impl_from_input(collective_impl_str);
      all_to_all_implementation_per_dimension.push_back(ci);
    }
  }
  if (j.contains("collective-optimization")) {
    string inp_collective_optimization = j["collective-optimization"];
    if (inp_collective_optimization == "baseline") {
      collectiveOptimization = CollectiveOptimization::Baseline;
    } else if (inp_collective_optimization == "localBWAware") {
      collectiveOptimization = CollectiveOptimization::LocalBWAware;
    } else {
      sys_panic("unknown value for collective optimization in sys input file");
    }
  }
  if (j.contains("local-reduction-delay")) {
    local_reduction_delay = j["local-reduction-delay"];
  }
  if (j.contains("active-chunks-per-dimension")) {
    active_chunks_per_dimension = j["active-chunks-per-dimension"];
  }
  if (j.contains("L")) {
    inp_L = j["L"];
  }
  if (j.contains("o")) {
    inp_o = j["o"];
  }
  if (j.contains("g")) {
    inp_g = j["g"];
  }
  if (j.contains("G")) {
    inp_G = j["G"];
  }
  if (j.contains("endpoint-delay")) {
    communication_delay = j["endpoint-delay"];
    communication_delay = communication_delay * injection_scale;
  }
  if (j.contains("model-shared-bus")) {
    int inp_model_shared_bus = j["model-shared-bus"];
    if (inp_model_shared_bus == 1) {
      model_shared_bus = true;
    } else {
      model_shared_bus = false;
    }
  } else {
    model_shared_bus = false;
  }
  if (j.contains("preferred-dataset-splits")) {
    preferred_dataset_splits = j["preferred-dataset-splits"];
  }
  if (j.contains("peak-perf")) {
    peak_perf = j["peak-perf"];
    peak_perf = peak_perf * 1000000000000; // TFLOPS
  }
  if (j.contains("local-mem-bw")) {
    local_mem_bw = j["local-mem-bw"]; // GB/sec
  }
  if (j.contains("roofline-enabled")) {
    if (j["roofline-enabled"] != 0) {
      roofline_enabled = true;
      roofline = new Roofline(local_mem_bw, peak_perf);
    }
  }
  this->trace_enabled = false;
  if (j.contains("trace-enabled")) {
    if (j["trace-enabled"] != 0) {
      this->trace_enabled = true;
    } else {
      this->trace_enabled = false;
    }
  }
  this->replay_only = false;
  if (j.contains("replay-only")) {
    if (j["replay-only"] != 0) {
      this->replay_only = true;
    } else {
      this->replay_only = false;
    }
  }

  inFile.close();
  return true;
}

CollectiveImpl* Sys::generate_collective_impl_from_input(
    string collective_impl_str) {
  if (collective_impl_str == "ring") {
    return new CollectiveImpl(CollectiveImplType::Ring);
  } else if (collective_impl_str == "oneRing") {
    return new CollectiveImpl(CollectiveImplType::OneRing);
  } else if (collective_impl_str == "doubleBinaryTree") {
    return new CollectiveImpl(CollectiveImplType::DoubleBinaryTree);
  } else if (collective_impl_str.rfind("direct", 0) == 0) {
    int window = -1;
    if (collective_impl_str != "direct") {
      window = stoi(collective_impl_str.substr(6, 5));
    }
    return new DirectCollectiveImpl(CollectiveImplType::Direct, window);
  } else if (collective_impl_str.rfind("oneDirect", 0) == 0) {
    int window = -1;
    if (collective_impl_str != "oneDirect") {
      window = stoi(collective_impl_str.substr(9, 5));
    }
    return new DirectCollectiveImpl(CollectiveImplType::OneDirect, window);
  } else if (collective_impl_str == "halvingDoubling") {
    return new CollectiveImpl(CollectiveImplType::HalvingDoubling);
  } else if (collective_impl_str == "oneHalvingDoubling") {
    return new CollectiveImpl(CollectiveImplType::OneHalvingDoubling);
  } else {
    sys_panic(
        "Cannot interpret collective implementations. Please check the collective implementations in the sys"
        "input file");
    return new CollectiveImpl(CollectiveImplType::Ring);
  }
}

Tick Sys::boostedTick() {
  Sys* ts = all_sys[0];
  if (ts == nullptr) {
    for (uint64_t i = 1; i < all_sys.size(); i++) {
      if (all_sys[i] != nullptr) {
        ts = all_sys[i];
        break;
      }
    }
  }
  timespec_t tmp = ts->comm_NI->sim_get_time();
  Tick tick = tmp.time_val / CLOCK_PERIOD;
  return tick;
}

void Sys::sys_panic(string msg) {
  auto logger = LoggerFactory::get_logger("system");
  logger->critical(msg);
  exit(1);
}

void Sys::exit_sim_loop(string msg) {
  auto logger = LoggerFactory::get_logger("system");
  logger->warn(msg);
}

void Sys::call(EventType type, CallData* data) {}

void Sys::call_events() {
  for (auto& callable : event_queue[Sys::boostedTick()]) {
    try {
      pending_events--;
      (get<0>(callable))->call(get<1>(callable), get<2>(callable));
    } catch (const std::exception& e) {
      auto logger = LoggerFactory::get_logger("system");
      logger->critical(
          "warning! a callable is removed before call {}", e.what());
    }
  }
  if (event_queue[Sys::boostedTick()].size() > 0) {
    event_queue[Sys::boostedTick()].clear();
  }
  event_queue.erase(Sys::boostedTick());
}

void Sys::register_event(
    Callable* callable,
    EventType event,
    CallData* callData,
    Tick delta_cycles) {
  try_register_event(callable, event, callData, delta_cycles);
}

void Sys::try_register_event(
    Callable* callable,
    EventType event,
    CallData* callData,
    Tick& delta_cycles) {
  bool should_schedule = false;
  auto event_time = Sys::boostedTick() + delta_cycles;
  if (event_queue.find(event_time) == event_queue.end()) {
    list<tuple<Callable*, EventType, CallData*>> tmp;
    event_queue[event_time] = tmp;
    should_schedule = true;
  }
  event_queue[event_time].push_back(make_tuple(callable, event, callData));
  if (should_schedule) {
    timespec_t tmp;
    tmp.time_res = NS;
    tmp.time_val = delta_cycles;
    BasicEventHandlerData* data =
        new BasicEventHandlerData(id, EventType::CallEvents);
    data->sys_id = id;
    comm_NI->sim_schedule(tmp, &Sys::handleEvent, data);
  }
  delta_cycles = 0;
  pending_events++;
  return;
}

void Sys::handleEvent(void* arg) {
  if (arg == nullptr) {
    return;
  }
  BasicEventHandlerData* ehd = (BasicEventHandlerData*)arg;
  int id = ehd->sys_id;
  EventType event = ehd->event;

  if (event == EventType::CallEvents) {
    all_sys[id]->call_events();
    delete ehd;
  } else if (
      (event == EventType::NPU_to_MA) || (event == EventType::MA_to_NPU)) {
    all_sys[id]->call_events();
  } else if (event == EventType::RendezvousSend) {
    RendezvousSendData* rsd = (RendezvousSendData*)ehd;
    rsd->send.call(EventType::General, nullptr);
    delete rsd;
  } else if (event == EventType::RendezvousRecv) {
    RendezvousRecvData* rrd = (RendezvousRecvData*)ehd;
    rrd->recv.call(EventType::General, nullptr);
    delete rrd;
  } else if (
      (event == EventType::CompFinished) ||
      (event == EventType::MemLoadFinished) ||
      (event == EventType::MemStoreFinished)) {
    MemEventHandlerData* mehd = (MemEventHandlerData*)ehd;
    if (mehd->workload) {
      mehd->workload->call(event, mehd->wlhd);
    }
    delete mehd;
  } else if (event == EventType::PacketReceived) {
    RecvPacketEventHandlerData* rcehd = (RecvPacketEventHandlerData*)ehd;
    if (rcehd->workload) {
      rcehd->workload->call(event, rcehd->wlhd);
    }
    if (rcehd->owner) {
      rcehd->owner->consume(rcehd);
    }
    delete rcehd;
  } else if (event == EventType::PacketSent) {
    SendPacketEventHandlerData* sehd = (SendPacketEventHandlerData*)ehd;
    sehd->callable->call(EventType::PacketSent, sehd->wlhd);
    delete sehd;
  }
}

LogicalTopology* Sys::get_logical_topology(ComType comm_type) {
  if (comm_type == ComType::All_Reduce)
    return logical_topologies["AllReduce"];
  else if (comm_type == ComType::All_to_All)
    return logical_topologies["AllToAll"];
  else if (comm_type == ComType::Reduce_Scatter)
    return logical_topologies["ReduceScatter"];
  else if (comm_type == ComType::All_Gather)
    return logical_topologies["AllGather"];
  else {
    sys_panic("no known logical topology!");
    return nullptr;
  }
}

vector<CollectiveImpl*> Sys::get_collective_implementation(ComType comm_type) {
  if (comm_type == ComType::All_Reduce)
    return all_reduce_implementation_per_dimension;
  else if (comm_type == ComType::All_to_All)
    return all_to_all_implementation_per_dimension;
  else if (comm_type == ComType::Reduce_Scatter)
    return reduce_scatter_implementation_per_dimension;
  else if (comm_type == ComType::All_Gather)
    return all_gather_implementation_per_dimension;
  else {
    sys_panic("no known collective implementation!");
    vector<CollectiveImpl*> tmp;
    return tmp;
  }
}

DataSet* Sys::generate_all_reduce(
    uint64_t size,
    vector<bool> involved_dimensions,
    CommunicatorGroup* communicator_group,
    int explicit_priority) {
  if (communicator_group == nullptr) {
    return generate_collective(
        size,
        logical_topologies["AllReduce"],
        all_reduce_implementation_per_dimension,
        involved_dimensions,
        ComType::All_Reduce,
        explicit_priority,
        communicator_group);
  } else {
    CollectivePlan* plan =
        communicator_group->get_collective_plan(ComType::All_Reduce);
    return generate_collective(
        size,
        plan->topology,
        plan->implementation_per_dimension,
        plan->dimensions_involved,
        ComType::All_Reduce,
        explicit_priority,
        communicator_group);
  }
}

DataSet* Sys::generate_all_to_all(
    uint64_t size,
    vector<bool> involved_dimensions,
    CommunicatorGroup* communicator_group,
    int explicit_priority) {
  if (communicator_group == nullptr) {
    return generate_collective(
        size,
        logical_topologies["AllToAll"],
        all_to_all_implementation_per_dimension,
        involved_dimensions,
        ComType::All_to_All,
        explicit_priority,
        communicator_group);
  } else {
    CollectivePlan* plan =
        communicator_group->get_collective_plan(ComType::All_to_All);
    return generate_collective(
        size,
        plan->topology,
        plan->implementation_per_dimension,
        plan->dimensions_involved,
        ComType::All_to_All,
        explicit_priority,
        communicator_group);
  }
}

DataSet* Sys::generate_all_gather(
    uint64_t size,
    vector<bool> involved_dimensions,
    CommunicatorGroup* communicator_group,
    int explicit_priority) {
  if (communicator_group == nullptr) {
    return generate_collective(
        size,
        logical_topologies["AllGather"],
        all_gather_implementation_per_dimension,
        involved_dimensions,
        ComType::All_Gather,
        explicit_priority,
        communicator_group);
  } else {
    CollectivePlan* plan =
        communicator_group->get_collective_plan(ComType::All_Gather);
    return generate_collective(
        size,
        plan->topology,
        plan->implementation_per_dimension,
        plan->dimensions_involved,
        ComType::All_Gather,
        explicit_priority,
        communicator_group);
  }
}

DataSet* Sys::generate_reduce_scatter(
    uint64_t size,
    vector<bool> involved_dimensions,
    CommunicatorGroup* communicator_group,
    int explicit_priority) {
  if (communicator_group == nullptr) {
    return generate_collective(
        size,
        logical_topologies["ReduceScatter"],
        reduce_scatter_implementation_per_dimension,
        involved_dimensions,
        ComType::Reduce_Scatter,
        explicit_priority,
        communicator_group);
  } else {
    CollectivePlan* plan =
        communicator_group->get_collective_plan(ComType::Reduce_Scatter);
    return generate_collective(
        size,
        plan->topology,
        plan->implementation_per_dimension,
        plan->dimensions_involved,
        ComType::Reduce_Scatter,
        explicit_priority,
        communicator_group);
  }
}

DataSet* Sys::generate_collective(
    uint64_t size,
    LogicalTopology* topology,
    vector<CollectiveImpl*> implementation_per_dimension,
    vector<bool> dimensions_involved,
    ComType collective_type,
    int explicit_priority,
    CommunicatorGroup* communicator_group) {
  uint64_t chunk_size = determine_chunk_size(size, collective_type);
  uint64_t recommended_chunk_size = chunk_size;
  int streams = ceil(((double)size) / chunk_size);
  uint64_t remain_size;
  DataSet* dataset = new DataSet(streams);
  int pri = get_priority(explicit_priority);
  int count = 0;
  if (id == 0 &&
      (inter_dimension_scheduling == InterDimensionScheduling::OfflineGreedy ||
       inter_dimension_scheduling ==
           InterDimensionScheduling::OfflineGreedyFlex)) {
    if (last_scheduled_collective != Sys::boostedTick()) {
      offline_greedy->reset_loads();
      last_scheduled_collective = Sys::boostedTick();
    }
  }

  while (size > 0) {
    count++;

    vector<int> dim_mapper(topology->get_num_of_dimensions());
    iota(begin(dim_mapper), end(dim_mapper), 0);
    if (collective_type == ComType::All_Gather) {
      reverse(dim_mapper.begin(), dim_mapper.end());
    }

    if (inter_dimension_scheduling == InterDimensionScheduling::RoundRobin) {
      rotate(
          dim_mapper.begin(),
          dim_mapper.begin() + round_robin_inter_dimension_scheduler,
          dim_mapper.end());
      round_robin_inter_dimension_scheduler++;
      if (round_robin_inter_dimension_scheduler ==
          topology->get_num_of_dimensions()) {
        round_robin_inter_dimension_scheduler = 0;
      }
    } else if (
        collective_type != ComType::All_to_All &&
        (inter_dimension_scheduling ==
             InterDimensionScheduling::OfflineGreedy ||
         inter_dimension_scheduling ==
             InterDimensionScheduling::OfflineGreedyFlex)) {
      uint64_t prev_size = size;
      dim_mapper = offline_greedy->get_chunk_scheduling(
          num_streams,
          size,
          recommended_chunk_size,
          dimensions_involved,
          inter_dimension_scheduling,
          collective_type);
      chunk_size = prev_size - size;
    }

    if (collective_type == ComType::All_to_All ||
        (inter_dimension_scheduling !=
             InterDimensionScheduling::OfflineGreedy &&
         inter_dimension_scheduling !=
             InterDimensionScheduling::OfflineGreedyFlex)) {
      if (chunk_size > size) size = 0;
      else size -= chunk_size;
    }
    remain_size = chunk_size;
    list<CollectivePhase> vect;

    if (collective_type != ComType::All_Reduce ||
        collectiveOptimization == CollectiveOptimization::Baseline) {
      for (int dim = 0; dim < topology->get_num_of_dimensions(); dim++) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1 ||
            !dimensions_involved[dim_mapper[dim]]) {
          continue;
        }
        pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            collective_type,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], collective_type),
            remain_size,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]]);
        vect.push_back(phase);
        remain_size = phase.final_data_size;
      }
    } else if (
        inter_dimension_scheduling == InterDimensionScheduling::OfflineGreedy ||
        inter_dimension_scheduling ==
            InterDimensionScheduling::OfflineGreedyFlex ||
        inter_dimension_scheduling == InterDimensionScheduling::OnlineGreedy) {
      int dim = 0;

      // Create collective phase for each dimension in ascending order.
      for (dim = 0; dim < topology->get_num_of_dimensions(); dim++) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1 ||
            !dimensions_involved[dim_mapper[dim]]) {
          continue;
        }
        pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level_first(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            ComType::Reduce_Scatter,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], ComType::Reduce_Scatter),
            remain_size,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]]);
        vect.push_back(phase);
        remain_size = phase.final_data_size;
      }
      dim--;

      // Create collective phases for each dimension in descending order.
      for (; dim >= 0; dim--) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1 ||
            !dimensions_involved[dim_mapper[dim]]) {
          continue;
        }
        pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level_last(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            ComType::All_Gather,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], ComType::All_Gather),
            remain_size,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]]);
        vect.push_back(phase);
        remain_size = phase.final_data_size;
      }
    } else {
      // In this branch, and the branch directly above, a collective visits each
      // dimension (excluding the last dimension) twice. Specifically, for
      // example, in 2D AllReduce, there would be 3 collective phases: Phase 0:
      // Reduce Scatter in dim 0, Phase 1: All Gather in dim 1, Phase 2: All
      // Reduce in dim 2 Similarly, in 3D AllReduce, there would be 5 collective
      // phases: RS in dim 0, RS in dim 1, AG in dim 2, AR in dim 3, AR in
      // dim 4. Currently, queues are allocated per dimension. If we allocate
      // all queues in a dimension to both phases of a single dimension, a race
      // / deadlock condition may occur. Therefore, in these cases, we have to
      // allocate half of the queues to the first phase, and the remaining half
      // to the second phase. (For example, in the above 2D case, if we have 4
      // queues per dim, queues 0~1 are allocated to phase 0, queues 2~3 are
      // allocated to phase 2. For details, refer to
      // https://github.com/astra-sim/astra-sim/issues/137 and the linked
      // document.

      int dim = 0;
      int last_active_dim = 0;
      for (dim = 0; dim < topology->get_num_of_dimensions(); dim++) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) != 1 &&
            dimensions_involved[dim_mapper[dim]]) {
          last_active_dim = dim;
        }
      }

      // Create collective phase for each dimension, excluding the last
      // dimension, in ascending order.
      for (dim = 0; dim < last_active_dim; dim++) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1 ||
            !dimensions_involved[dim_mapper[dim]]) {
          continue;
        }
        // Allocate the first half of queues available to this dimension.
        pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level_first(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            ComType::Reduce_Scatter,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], ComType::Reduce_Scatter),
            remain_size,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]]);
        vect.push_back(phase);
        remain_size = phase.final_data_size;
      }
      while (dim > 0 &&
             (dimensions_involved[dim_mapper[dim]] == false ||
              topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1)) {
        dim--;
      }

      // The last dimension is the 'turning point'. Only one collective phase is
      // created.
      if (dimensions_involved[dim_mapper[dim]] &&
          topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) > 1) {
        // Despite only one collective phase being allocated to the last
        // dimension, we only allocate half of the queues available to this
        // dimension. This is because we want to match the number of queues
        // allocated to each collective phase. Processing phases for this dim in
        // n parallel queues, and queueing the next phases in n/2 parallel
        // queues could cause another deadlock. Refer to the PR #135 for more
        // details.
        pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level_first(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            ComType::All_Reduce,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], ComType::All_Reduce),
            remain_size,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]]);
        vect.push_back(phase);
        remain_size = phase.final_data_size;
      }
      dim--;

      // Create collective phases for each dimension, excluding the last
      // dimension, in descending order.
      for (; dim >= 0; dim--) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1 ||
            !dimensions_involved[dim_mapper[dim]]) {
          continue;
        }
        // Allocate the second half of queues available to this dimension.
        pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level_last(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            ComType::All_Gather,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], ComType::All_Gather),
            remain_size,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]]);
        vect.push_back(phase);
        remain_size = phase.final_data_size;
      }
    }
    if (vect.size() > 0) {
      int stream_id = num_streams++;
      if (communicator_group != nullptr) {
        stream_id = communicator_group->num_streams++;
      }
      StreamBaseline* newStream =
          new StreamBaseline(this, dataset, stream_id, vect, pri);
      newStream->current_queue_id = -1;
      insert_into_ready_list(newStream);
    } else {
      dataset->active = false;
      break;
    }
  }
  if (dataset->active) {
    dataset->total_streams = count;
  }
  return dataset;
}

CollectivePhase Sys::generate_collective_phase(
    ComType collective_type,
    BasicLogicalTopology* topology,
    uint64_t data_size,
    int queue_id,
    RingTopology::Direction direction,
    InjectionPolicy injection_policy,
    CollectiveImpl* collective_impl) {
  if (collective_impl->type == CollectiveImplType::Ring ||
      collective_impl->type == CollectiveImplType::OneRing) {
    CollectivePhase vn(
        this,
        queue_id,
        new Ring(
            collective_type,
            id,
            (RingTopology*)topology,
            data_size,
            direction,
            injection_policy));
    return vn;
  } else if (
      collective_impl->type == CollectiveImplType::Direct ||
      collective_impl->type == CollectiveImplType::OneDirect) {
    CollectivePhase vn(
        this,
        queue_id,
        new AllToAll(
            collective_type,
            ((DirectCollectiveImpl*)collective_impl)->direct_collective_window,
            id,
            (RingTopology*)topology,
            data_size,
            direction,
            InjectionPolicy::Normal));
    return vn;
  } else if (collective_impl->type == CollectiveImplType::DoubleBinaryTree) {
    CollectivePhase vn(
        this,
        queue_id,
        new DoubleBinaryTreeAllReduce(id, (BinaryTree*)topology, data_size));
    return vn;
  } else if (
      collective_impl->type == CollectiveImplType::HalvingDoubling ||
      collective_impl->type == CollectiveImplType::OneHalvingDoubling) {
    CollectivePhase vn(
        this,
        queue_id,
        new HalvingDoubling(
            collective_type, id, (RingTopology*)topology, data_size));
    return vn;
  } else {
    LoggerFactory::get_logger("system")->critical(
        "Error: No known collective implementation for collective phase");
    exit(1);
  }
}

int Sys::break_dimension(int model_parallel_npu_group) {
  if (break_dimension_done) {
    return dimension_to_break;
  }

  dimension_to_break = 0;
  if (model_parallel_npu_group == 1) {
    break_dimension_done = true;
    return -1;
  }
  int all_npus = 1;
  for (; dimension_to_break < physical_dims.size(); dimension_to_break++) {
    if (all_npus * physical_dims[dimension_to_break] <
        model_parallel_npu_group) {
      all_npus *= physical_dims[dimension_to_break];
    } else if (
        all_npus * physical_dims[dimension_to_break] >
        model_parallel_npu_group) {
      for (auto lt : logical_topologies) {
        delete lt.second;
      }
      logical_topologies.clear();

      delete scheduler_unit;
      delete vLevels;
      std::vector<int>::iterator levelIterator = queues_per_dim.begin();
      std::advance(levelIterator, dimension_to_break);
      queues_per_dim.insert(levelIterator, queues_per_dim[dimension_to_break]);
      scheduler_unit = new SchedulerUnit(
          this,
          queues_per_dim,
          max_running,
          active_first_phase,
          concurrent_streams);
      vLevels = new QueueLevels(queues_per_dim, 0, comm_NI->get_backend_type());

      int first_subdim = model_parallel_npu_group / all_npus;
      int second_subdim = physical_dims[dimension_to_break] / first_subdim;
      std::vector<int> logical_dims;

      for (uint64_t dim = 0; dim < physical_dims.size(); dim++) {
        if (dim != static_cast<uint64_t>(dimension_to_break)) {
          logical_dims.push_back(physical_dims[dim]);
        } else {
          logical_dims.push_back(first_subdim);
          logical_dims.push_back(second_subdim);
        }
      }

      std::vector<CollectiveImpl*>::iterator it =
          all_reduce_implementation_per_dimension.begin();
      if (all_reduce_implementation_per_dimension.size() > dimension_to_break) {
        std::advance(it, dimension_to_break);
      } else {
        std::advance(it, all_reduce_implementation_per_dimension.size());
      }
      CollectiveImpl* replicate = (CollectiveImpl*)(*it)->clone();
      all_reduce_implementation_per_dimension.insert(it, replicate);

      it = reduce_scatter_implementation_per_dimension.begin();
      if (reduce_scatter_implementation_per_dimension.size() >
          dimension_to_break) {
        std::advance(it, dimension_to_break);
      } else {
        std::advance(it, reduce_scatter_implementation_per_dimension.size());
      }
      replicate = (CollectiveImpl*)(*it)->clone();
      reduce_scatter_implementation_per_dimension.insert(it, replicate);

      it = all_gather_implementation_per_dimension.begin();
      if (all_gather_implementation_per_dimension.size() > dimension_to_break) {
        std::advance(it, dimension_to_break);
      } else {
        std::advance(it, all_gather_implementation_per_dimension.size());
      }
      replicate = (CollectiveImpl*)(*it)->clone();
      all_gather_implementation_per_dimension.insert(it, replicate);

      it = all_to_all_implementation_per_dimension.begin();
      if (all_to_all_implementation_per_dimension.size() > dimension_to_break) {
        std::advance(it, dimension_to_break);
      } else {
        std::advance(it, all_to_all_implementation_per_dimension.size());
      }
      replicate = (CollectiveImpl*)(*it)->clone();
      all_to_all_implementation_per_dimension.insert(it, replicate);
      logical_topologies["AllReduce"] = new GeneralComplexTopology(
          id, logical_dims, all_reduce_implementation_per_dimension);
      logical_topologies["ReduceScatter"] = new GeneralComplexTopology(
          id, logical_dims, reduce_scatter_implementation_per_dimension);
      logical_topologies["AllGather"] = new GeneralComplexTopology(
          id, logical_dims, all_gather_implementation_per_dimension);
      logical_topologies["AllToAll"] = new GeneralComplexTopology(
          id, logical_dims, all_to_all_implementation_per_dimension);
      this->logical_broken_dims = logical_dims;
      this->dim_to_break = dimension_to_break;

      break_dimension_done = true;
      return dimension_to_break;
    } else if (
        all_npus * physical_dims[dimension_to_break] ==
        model_parallel_npu_group) {
      break_dimension_done = true;
      return dimension_to_break;
    }
  }

  break_dimension_done = true;
  return -1;
}

uint64_t Sys::determine_chunk_size(uint64_t& size, ComType type) {
  uint64_t chunk_size = size / preferred_dataset_splits;
  // We want the collective size to have minimum size, otherwise, there is a
  // possibility of size overflow due to further dividing it to more
  // fine-grained messages
  if (type != ComType::All_Gather && this->total_nodes > chunk_size) {
    chunk_size = this->total_nodes;
    size = preferred_dataset_splits * chunk_size;
  }
  return chunk_size;
}

int Sys::get_priority(int explicit_priority) {
  if (scheduling_policy == SchedulingPolicy::LIFO) {
    return priority_counter++;
  } else if (scheduling_policy == SchedulingPolicy::FIFO) {
    return priority_counter--;
  } else if (scheduling_policy == SchedulingPolicy::EXPLICIT) {
    return explicit_priority;
  }

  // should not reach here
  assert(false);
  std::exit(-1);
}

void Sys::insert_into_ready_list(BaseStream* stream) {
  insert_stream(&ready_list, stream);
  scheduler_unit->notify_stream_added_into_ready_list();
}

void Sys::insert_stream(list<BaseStream*>* queue, BaseStream* baseStream) {
  list<BaseStream*>::iterator it = queue->begin();
  if (intra_dimension_scheduling == IntraDimensionScheduling::FIFO ||
      baseStream->current_queue_id < 0 ||
      baseStream->current_com_type == ComType::All_to_All ||
      baseStream->current_com_type == ComType::All_Reduce) {
    while (it != queue->end()) {
      if ((*it)->initialized == true) {
        advance(it, 1);
        continue;
      } else if ((*it)->priority >= baseStream->priority) {
        advance(it, 1);
        continue;
      } else {
        break;
      }
    }
  } else if (intra_dimension_scheduling == IntraDimensionScheduling::RG) {
    ComType one_to_last = ComType::None;
    ComType last = ComType::None;
    while (it != queue->end()) {
      one_to_last = last;
      last = (*it)->current_com_type;
      if ((*it)->initialized == true) {
        advance(it, 1);
        if (it != queue->end() && (*it)->initialized == false) {
          one_to_last = last;
          last = (*it)->current_com_type;
          advance(it, 1);
        }
        continue;
      } else if ((*it)->priority > baseStream->priority) {
        advance(it, 1);
        continue;
      } else if (
          (last == ComType::Reduce_Scatter &&
           one_to_last == ComType::All_Gather) ||
          (last == ComType::All_Gather &&
           one_to_last == ComType::Reduce_Scatter)) {
        advance(it, 1);
        continue;
      } else {
        break;
      }
    }
  } else if (
      intra_dimension_scheduling == IntraDimensionScheduling::SmallestFirst) {
    if (baseStream->phases_to_go.size() == 1) {
      it = queue->end();
    }
    while (it != queue->end()) {
      if ((*it)->initialized == true) {
        advance(it, 1);
        continue;
      } else if (
          max((*it)->my_current_phase.initial_data_size,
              (*it)->my_current_phase.final_data_size) <
          max(baseStream->my_current_phase.initial_data_size,
              baseStream->my_current_phase.final_data_size)) {
        advance(it, 1);
        continue;
      } else {
        break;
      }
    }
  } else if (
      intra_dimension_scheduling ==
      IntraDimensionScheduling::LessRemainingPhaseFirst) {
    while (it != queue->end()) {
      if ((*it)->initialized == true) {
        advance(it, 1);
        continue;
      } else if ((*it)->phases_to_go.size() < baseStream->phases_to_go.size()) {
        advance(it, 1);
        continue;
      } else {
        break;
      }
    }
  }
  queue->insert(it, baseStream);
}

void Sys::ask_for_schedule(int max) {
  if (ready_list.size() == 0 ||
      ready_list.front()->synchronizer[ready_list.front()->stream_id] <
          all_sys.size()) {
    return;
  }
  int top = ready_list.front()->stream_id;
  uint64_t min = ready_list.size();
  if (min > max) {
    min = static_cast<uint64_t>(max);
  }
  for (auto& sys : all_sys) {
    if (sys->ready_list.size() == 0 ||
        sys->ready_list.front()->stream_id != top) {
      return;
    }
    if (sys->ready_list.size() < min) {
      min = sys->ready_list.size();
    }
  }
  for (auto& sys : all_sys) {
    sys->schedule(min);
  }
  return;
}

void Sys::schedule(int num) {
  int ready_list_size = ready_list.size();
  int counter = min(num, ready_list_size);
  while (counter > 0) {
    int top_vn = ready_list.front()->phases_to_go.front().queue_id;
    int total_waiting_streams = ready_list.size();
    int total_phases = ready_list.front()->phases_to_go.size();

    proceed_to_next_vnet_baseline((StreamBaseline*)ready_list.front());

    if (ready_list.front()->current_queue_id == -1) {
      Sys::sys_panic(
          "should not happen! " +
          to_string(BaseStream::synchronizer[ready_list.front()->stream_id]) +
          " , " +
          to_string(BaseStream::ready_counter[ready_list.front()->stream_id]) +
          " , top queue id: " + to_string(top_vn) +
          " , total phases: " + to_string(total_phases) +
          " , waiting streams: " + to_string(total_waiting_streams));
    }

    ready_list.pop_front();
    counter--;
    first_phase_streams++;
    total_running_streams++;
  }
}

void Sys::proceed_to_next_vnet_baseline(StreamBaseline* stream) {
  int previous_vnet = stream->current_queue_id;
  if (stream->steps_finished == 1) {
    first_phase_streams--;
  }
  if (stream->steps_finished != 0) {
    stream->net_message_latency.back() /= stream->net_message_counter;
  }
  if (stream->my_current_phase.algorithm != nullptr) {
    delete stream->my_current_phase.algorithm;
  }
  if (stream->phases_to_go.size() == 0) {
    stream->take_bus_stats_average();
    stream->dataset->notify_stream_finished((StreamStat*)stream);
  }
  if (stream->current_queue_id >= 0 && stream->my_current_phase.enabled) {
    list<BaseStream*>& target =
        active_Streams.at(stream->my_current_phase.queue_id);
    for (list<BaseStream*>::iterator it = target.begin(); it != target.end();
         ++it) {
      if (((StreamBaseline*)(*it))->stream_id == stream->stream_id) {
        target.erase(it);
        break;
      }
    }
  }
  if (stream->phases_to_go.size() == 0) {
    total_running_streams--;
    if (previous_vnet >= 0) {
      scheduler_unit->notify_stream_removed(
          previous_vnet, Sys::boostedTick() - stream->last_init);
    }
    delete stream;
    return;
  }
  stream->steps_finished++;
  // This is hot fix for random failures of simulation.
  // TODO: Need to find a better way for negative queue id occurrence.
  if (stream->phases_to_go.front().queue_id < 0)
    stream->phases_to_go.front().queue_id *= -1;
  stream->current_queue_id = stream->phases_to_go.front().queue_id;
  stream->current_com_type = stream->phases_to_go.front().comm_type;

  CollectivePhase vi = stream->phases_to_go.front();
  stream->my_current_phase = vi;
  stream->phases_to_go.pop_front();
  stream->test = 0;
  stream->test2 = 0;
  stream->initialized = false;
  stream->last_phase_change = Sys::boostedTick();
  stream->total_packets_sent = 0;

  stream->net_message_latency.push_back(0);
  stream->net_message_counter = 0;

  if (stream->my_current_phase.enabled) {
    insert_stream(&active_Streams[stream->current_queue_id], stream);
  }

  stream->state = StreamState::Ready;

  if (previous_vnet >= 0) {
    scheduler_unit->notify_stream_removed(
        previous_vnet, Sys::boostedTick() - stream->last_init);
  }
  scheduler_unit->notify_stream_added(stream->current_queue_id);
}

int Sys::front_end_sim_send(
    Tick delay,
    void* buffer,
    uint64_t count,
    int type,
    int dst,
    int tag,
    sim_request* request,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg) {
  if (rendezvous_enabled) {
    return rendezvous_sim_send(
        delay, buffer, count, type, dst, tag, request, msg_handler, fun_arg);
  } else {
    return sim_send(
        delay, buffer, count, type, dst, tag, request, msg_handler, fun_arg);
  }
}

int Sys::front_end_sim_recv(
    Tick delay,
    void* buffer,
    uint64_t count,
    int type,
    int src,
    int tag,
    sim_request* request,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg) {
  if (rendezvous_enabled) {
    return rendezvous_sim_recv(
        delay, buffer, count, type, src, tag, request, msg_handler, fun_arg);
  } else {
    return sim_recv(
        delay, buffer, count, type, src, tag, request, msg_handler, fun_arg);
  }
}

int Sys::rendezvous_sim_send(
    Tick delay,
    void* buffer,
    uint64_t count,
    int type,
    int dst,
    int tag,
    sim_request* request,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg) {
  RendezvousSendData* rsd = new RendezvousSendData(
      id, this, buffer, count, type, dst, tag, *request, msg_handler, fun_arg);
  sim_request newReq = *request;
  uint64_t rendevouz_size = 8192;
  newReq.dstRank = request->srcRank;
  newReq.srcRank = request->dstRank;
  newReq.reqCount = rendevouz_size;
  int newTag = tag + 500000000;
  newReq.tag = newTag;
  sim_recv(
      delay,
      buffer,
      rendevouz_size,
      type,
      dst,
      newTag,
      &newReq,
      &Sys::handleEvent,
      rsd);
  return 1;
}

int Sys::rendezvous_sim_recv(
    Tick delay,
    void* buffer,
    uint64_t count,
    int type,
    int src,
    int tag,
    sim_request* request,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg) {
  RendezvousRecvData* rrd = new RendezvousRecvData(
      id, this, buffer, count, type, src, tag, *request, msg_handler, fun_arg);
  sim_request newReq = *request;
  uint64_t rendevouz_size = 8192;
  newReq.dstRank = request->srcRank;
  newReq.srcRank = request->dstRank;
  newReq.reqCount = rendevouz_size;
  int newTag = tag + 500000000;
  newReq.tag = newTag;
  sim_send(
      delay,
      buffer,
      rendevouz_size,
      type,
      src,
      newTag,
      &newReq,
      &Sys::handleEvent,
      rrd);
  return 1;
}

int Sys::sim_send(
    Tick delay,
    void* buffer,
    uint64_t count,
    int type,
    int dst,
    int tag,
    sim_request* request,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg) {
  if (delay == 0) {
    comm_NI->sim_send(
        buffer, count, type, dst, tag, request, msg_handler, fun_arg);
  } else {
    try_register_event(
        new SimSendCaller(
            this,
            buffer,
            count,
            type,
            dst,
            tag,
            *request,
            msg_handler,
            fun_arg,
            true),
        EventType::General,
        nullptr,
        delay);
  }
  return 1;
}

int Sys::sim_recv(
    Tick delay,
    void* buffer,
    uint64_t count,
    int type,
    int src,
    int tag,
    sim_request* request,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg) {
  if (delay == 0) {
    comm_NI->sim_recv(
        buffer, count, type, src, tag, request, msg_handler, fun_arg);
  } else {
    try_register_event(
        new SimRecvCaller(
            this,
            buffer,
            count,
            type,
            src,
            tag,
            *request,
            msg_handler,
            fun_arg,
            true),
        EventType::General,
        nullptr,
        delay);
  }
  return 1;
}
} // namespace AstraSim
