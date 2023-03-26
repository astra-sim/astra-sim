/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "Sys.hh"
#include "BaseStream.hh"
#include "DataSet.hh"
#include "MemBus.hh"
#include "QueueLevels.hh"
#include "SimRecvCaller.hh"
#include "SimSendCaller.hh"
#include "StreamBaseline.hh"
#include "astra-sim/system/collective/AllToAll.hh"
#include "astra-sim/system/collective/DoubleBinaryTreeAllReduce.hh"
#include "astra-sim/system/collective/HalvingDoubling.hh"
#include "astra-sim/system/collective/Ring.hh"

#include "astra-sim/system/scheduling/OfflineGreedy.hh"
#include "astra-sim/system/topology/BasicLogicalTopology.hh"
#include "astra-sim/system/topology/DoubleBinaryTreeTopology.hh"
#include "astra-sim/system/topology/GeneralComplexTopology.hh"
#include "astra-sim/system/topology/LocalRingGlobalBinaryTree.hh"
#include "astra-sim/system/topology/LocalRingNodeA2AGlobalDBT.hh"
#include "astra-sim/system/topology/Torus3D.hh"

#include "RendezvousRecvData.hh"
#include "RendezvousSendData.hh"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace AstraSim {
Tick Sys::offset = 0;
uint8_t* Sys::dummy_data = new uint8_t[2];
std::vector<Sys*> Sys::all_generators;

Sys::~Sys() {
  end_sim_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::minutes>(
      end_sim_time - start_sim_time);
  if (id == 0) {
    auto timenow =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << "*****" << std::endl
              << "Time to exit: " << ctime(&timenow)
              << "all-reduce Collective implementation: "
              << inp_all_reduce_implementation << std::endl
              << "reduce-scatter Collective implementation: "
              << inp_reduce_scatter_implementation << std::endl
              << "all-gather Collective implementation: "
              << inp_all_gather_implementation << std::endl
              << "all-to-all Collective implementation: "
              << inp_all_to_all_implementation << std::endl
              << "Collective optimization: " << inp_collective_optimization
              << std::endl
              << "Total sim duration: " << duration.count() / 60 << ":"
              << duration.count() % 60 << " hours" << std::endl
              << "Total streams injected: " << streams_injected << std::endl
              << "Total streams finished: " << streams_finished << std::endl
              << "Percentage of finished streams: "
              << (((double)streams_finished) / streams_injected) * 100 << " %"
              << std::endl
              << "*****" << std::endl;
  }
  all_generators[id] = nullptr;
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
  for (auto& a : all_generators) {
    if (a != nullptr) {
      shouldExit = false;
      break;
    }
  }
  if (shouldExit) {
    exitSimLoop("Exiting");
  }
}

// Sys::Sys(AstraNetworkAPI *NI,int id,int num_passes,std::string
// my_sys,std::string my_workload,int total_stat_rows,int stat_row,std::string
// path,std::string run_name)
Sys::Sys(
    AstraNetworkAPI* NI,
    AstraMemoryAPI* MEM,
    int id,
    int num_passes,
    std::vector<int> physical_dims,
    std::vector<int> queues_per_dim,
    std::string my_sys,
    std::string my_workload,
    float comm_scale,
    float compute_scale,
    float injection_scale,
    int total_stat_rows,
    int stat_row,
    std::string path,
    std::string run_name,
    bool seprate_log,
    bool rendezvous_enabled) {
  scheduler_unit = nullptr;
  vLevels = nullptr;
  memBus = nullptr;
  workload = nullptr;
  offline_greedy = nullptr;
  this->initialized = false;
  this->intra_dimension_scheduling = IntraDimensionScheduling::FIFO;
  this->inter_dimension_scheduling = InterDimensionScheduling::Ascending;
  round_robin_inter_dimension_scheduler = 0;
  this->last_scheduled_collective = 0;
  this->dim_to_break = -1;

  start_sim_time = std::chrono::high_resolution_clock::now();
  this->NI = NI;
  this->MEM = MEM;
  this->id = id;
  this->method = "baseline";
  this->finished_workloads = 0;
  this->streams_finished = 0;
  this->streams_injected = 0;
  this->first_phase_streams = 0;
  this->total_running_streams = 0;
  this->priority_counter = 0;
  this->comm_scale = comm_scale;
  this->compute_scale = compute_scale;
  this->injection_scale = injection_scale;
  this->inp_model_shared_bus = 0;
  this->inp_boost_mode = 0;
  this->processing_latency = 10;
  this->communication_delay = 10;
  this->local_reduction_delay = 1;
  this->active_chunks_per_dimension = 1;
  this->seprate_log = seprate_log;
  this->rendezvous_enabled = rendezvous_enabled;
  if ((id + 1) > all_generators.size()) {
    all_generators.resize(id + 1);
  }
  all_generators[id] = this;

  this->gap_denominator = 16;

  bool result = initialize_sys(my_sys);

  if (result == false) {
    sys_panic(
        "Unable to initialize the system layer because the file can not be openned");
  }

  /*if (id == 0) {
    std::cout << "loc dim: " << first_dim
              << " ,vert dim: " << second_dim
              << " ,horiz dim: " << third_dim
              << " ,perpend dim: " << fourth_dim
              << " ,fourth dim: " << fifth_dim
              << " ,queues: " << first_queues << ", " << second_queues << ", "
              << third_queues << std::endl;
  }*/

  this->pending_events = 0;

  int total_disabled = 0;
  this->physical_dims = physical_dims;
  this->queues_per_dim = queues_per_dim;
  int element = 0;
  all_queues = 0;
  total_nodes = 1;
  for (int current_dim = 0; current_dim < queues_per_dim.size();
       current_dim++) {
    all_queues += queues_per_dim[current_dim];
    bool enabled = !boost_mode;
    if (id % total_nodes == 0 &&
        id < total_nodes * physical_dims[current_dim]) {
      enabled = true;
    }
    if (!enabled) {
      total_disabled += queues_per_dim[current_dim];
    }
    if (physical_dims[current_dim] >= 1) {
      total_nodes *= physical_dims[current_dim];
    }
    for (int j = 0; j < queues_per_dim[current_dim]; j++) {
      std::list<BaseStream*> temp;
      active_Streams[element] = temp;
      std::list<int> pri;
      stream_priorities[element] = pri;
      element++;
    }
  }
  if (all_queues == total_disabled) {
    NI->enabled = false;
    std::cout << "Node " << id << " has been totally disabled" << std::endl;
  }
  concurrent_streams =
      (int)std::ceil(((double)active_chunks_per_dimension) / queues_per_dim[0]);
  active_first_phase = 100000000;
  if (id == 0) {
    std::cout
        << "The final active chunks per dimension after allocating to queues is: "
        << active_first_phase << std::endl;
  }
  max_running = 100000000;
  scheduler_unit = new SchedulerUnit(
      this,
      queues_per_dim,
      max_running,
      active_first_phase,
      concurrent_streams);
  vLevels = new QueueLevels(queues_per_dim, 0, NI->get_backend_type());

  logical_topologies["AllReduce"] = new GeneralComplexTopology(
      id, physical_dims, all_reduce_implementation_per_dimension);
  logical_topologies["ReduceScatter"] = new GeneralComplexTopology(
      id, physical_dims, reduce_scatter_implementation_per_dimension);
  logical_topologies["AllGather"] = new GeneralComplexTopology(
      id, physical_dims, all_gather_implementation_per_dimension);
  logical_topologies["AllToAll"] = new GeneralComplexTopology(
      id, physical_dims, all_to_all_implementation_per_dimension);

  stream_counter = 0;

  if (id == 0) {
    std::atexit(exiting);
    std::cout << "total nodes: " << total_nodes << std::endl;
    /*std::cout << "local dim: " << first_dim
              << " , vertical dim: " << second_dim
              << " , horizontal dim: " << third_dim
              << " , perpendicular dim: " << fourth_dim
              << " , fourth dim: " << fifth_dim << std::endl;*/
  }
  // NI->sim_init(); CHANGED BY PALLAVI**
  NI->sim_init(MEM);
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
  workload = new Workload(
      run_name,
      this,
      my_workload,
      num_passes,
      total_stat_rows,
      stat_row,
      path,
      this->seprate_log);
  if (workload->initialized == false) {
    sys_panic(
        "Unable to initialize the workload layer because it can not open the workload file");
    return;
  }
  if (inter_dimension_scheduling == InterDimensionScheduling::OfflineGreedy ||
      inter_dimension_scheduling ==
          InterDimensionScheduling::OfflineGreedyFlex) {
    offline_greedy = new OfflineGreedy(this);
  }
  this->initialized = true;
}
int Sys::break_dimension(int model_parallel_npu_group) {
  if (model_parallel_npu_group == 1) {
    return -1;
  }
  int dimension_to_break = 0;
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
      vLevels = new QueueLevels(queues_per_dim, 0, NI->get_backend_type());

      int first_subdim = model_parallel_npu_group / all_npus;
      int second_subdim = physical_dims[dimension_to_break] / first_subdim;
      std::vector<int> logical_dims;

      for (int dim = 0; dim < physical_dims.size(); dim++) {
        if (dim != dimension_to_break) {
          logical_dims.push_back(physical_dims[dim]);
        } else {
          logical_dims.push_back(first_subdim);
          logical_dims.push_back(second_subdim);
        }
      }

      std::vector<CollectiveImplementation*>::iterator it =
          all_reduce_implementation_per_dimension.begin();
      if (all_reduce_implementation_per_dimension.size() > dimension_to_break) {
        std::advance(it, dimension_to_break);
      } else {
        std::advance(it, all_reduce_implementation_per_dimension.size());
      }
      CollectiveImplementation* replicate =
          (CollectiveImplementation*)(*it)->clone();
      all_reduce_implementation_per_dimension.insert(it, replicate);

      it = reduce_scatter_implementation_per_dimension.begin();
      if (reduce_scatter_implementation_per_dimension.size() >
          dimension_to_break) {
        std::advance(it, dimension_to_break);
      } else {
        std::advance(it, reduce_scatter_implementation_per_dimension.size());
      }
      replicate = (CollectiveImplementation*)(*it)->clone();
      reduce_scatter_implementation_per_dimension.insert(it, replicate);

      it = all_gather_implementation_per_dimension.begin();
      if (all_gather_implementation_per_dimension.size() > dimension_to_break) {
        std::advance(it, dimension_to_break);
      } else {
        std::advance(it, all_gather_implementation_per_dimension.size());
      }
      replicate = (CollectiveImplementation*)(*it)->clone();
      all_gather_implementation_per_dimension.insert(it, replicate);

      it = all_to_all_implementation_per_dimension.begin();
      if (all_to_all_implementation_per_dimension.size() > dimension_to_break) {
        std::advance(it, dimension_to_break);
      } else {
        std::advance(it, all_to_all_implementation_per_dimension.size());
      }
      replicate = (CollectiveImplementation*)(*it)->clone();
      all_to_all_implementation_per_dimension.insert(it, replicate);
      /*std::cout<<"logical dims: "<<std::endl;
      for(auto a:logical_dims){
        std::cout<<a<<", ";
      }
      std::cout<<std::endl;
      std::cout<<"implementation per dims: "<<std::endl;
      for(auto a:all_reduce_implementation_per_dimension){
        if(a==CollectiveImplementation::Ring){
          std::cout<<"ring"<<", ";
        }
        else if(a==CollectiveImplementation::Direct){
          std::cout<<"direct"<<", ";
        }
      }*/
      std::cout << std::endl;
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

      return dimension_to_break;
    } else if (
        all_npus * physical_dims[dimension_to_break] ==
        model_parallel_npu_group) {
      return dimension_to_break;
    }
  }
  return -1;
}
int Sys::get_layer_numbers(std::string workload_input) {
  return Workload::get_layer_numbers(workload_input);
}
int Sys::get_priority(SchedulingPolicy pref_scheduling) {
  if (pref_scheduling == SchedulingPolicy::None) {
    if (scheduling_policy == SchedulingPolicy::LIFO) {
      return priority_counter++;
    } else {
      return priority_counter--;
    }
  } else if (pref_scheduling == SchedulingPolicy::HIGHEST) {
    return 100000000;
  } else {
    if (scheduling_policy == SchedulingPolicy::LIFO) {
      return priority_counter++;
    } else {
      return priority_counter--;
    }
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
    NI->sim_send(buffer, count, type, dst, tag, request, msg_handler, fun_arg);
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
            fun_arg),
        EventType::General,
        nullptr,
        delay);
  }
  return 1;
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
  /*if(count<=0){
    std::cout<<"send from: "<<id<<" to: "<<dst<<" at time: "<<boostedTick()
              <<" ,size: "<<count<<std::endl;
    sys_panic("below zero send!");
  }*/
  if (rendezvous_enabled) {
    return rendezvous_sim_send(
        delay, buffer, count, type, dst, tag, request, msg_handler, fun_arg);
  } else {
    return sim_send(
        delay, buffer, count, type, dst, tag, request, msg_handler, fun_arg);
  }
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
    NI->sim_recv(buffer, count, type, src, tag, request, msg_handler, fun_arg);
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
            fun_arg),
        EventType::General,
        nullptr,
        delay);
  }
  return 1;
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
  /*if(count<=0){
    std::cout<<"recv at: "<<id<<" expecting data from: "<<src<<" at time:
  "<<boostedTick()
    <<" ,size: "<<count<<std::endl;
    sys_panic("below zero recv!");
  }*/
  if (rendezvous_enabled) {
    return rendezvous_sim_recv(
        delay, buffer, count, type, src, tag, request, msg_handler, fun_arg);
  } else {
    return sim_recv(
        delay, buffer, count, type, src, tag, request, msg_handler, fun_arg);
  }
}
Tick Sys::mem_read(uint64_t bytes) {
  if (MEM == nullptr) {
    return 10;
  }
  uint64_t delay_ns = MEM->npu_mem_read(bytes);
  Tick delay_cycles = delay_ns / CLOCK_PERIOD;
  return delay_cycles;
}
Tick Sys::mem_write(uint64_t bytes) {
  if (MEM == nullptr) {
    return 10;
  }
  uint64_t delay_ns = MEM->npu_mem_write(bytes);
  Tick delay_cycles = delay_ns / CLOCK_PERIOD;
  return delay_cycles;
}
std::string Sys::trim(
    const std::string& str,
    const std::string& whitespace = " \t") {
  const auto strBegin = str.find_first_not_of(whitespace);
  if (strBegin == std::string::npos)
    return ""; // no content

  const auto strEnd = str.find_last_not_of(whitespace);
  const auto strRange = strEnd - strBegin + 1;

  return str.substr(strBegin, strRange);
}
std::vector<CollectiveImplementation*> Sys::
    generate_collective_implementation_from_input(std::string input) {
  std::vector<std::string> inputs_per_dimension = split_string(input, "_");
  std::vector<CollectiveImplementation*> result;
  for (std::string dimension_input : inputs_per_dimension) {
    if (dimension_input == "ring") {
      result.push_back(
          new CollectiveImplementation(CollectiveImplementationType::Ring));
    } else if (dimension_input == "oneRing") {
      result.push_back(
          new CollectiveImplementation(CollectiveImplementationType::OneRing));
    } else if (dimension_input == "doubleBinaryTree") {
      result.push_back(new CollectiveImplementation(
          CollectiveImplementationType::DoubleBinaryTree));
    } else if (dimension_input.rfind("direct", 0) == 0) {
      int window = -1;
      if (dimension_input != "direct") {
        window = std::stoi(dimension_input.substr(6, 10));
      }
      result.push_back(new DirectCollectiveImplementation(
          CollectiveImplementationType::Direct, window));
    } else if (dimension_input.rfind("oneDirect", 0) == 0) {
      int window = -1;
      if (dimension_input != "oneDirect") {
        window = std::stoi(dimension_input.substr(9, 10));
      }
      result.push_back(new DirectCollectiveImplementation(
          CollectiveImplementationType::OneDirect, window));
    } else if (dimension_input == "halvingDoubling") {
      result.push_back(new CollectiveImplementation(
          CollectiveImplementationType::HalvingDoubling));
    } else if (dimension_input == "oneHalvingDoubling") {
      result.push_back(new CollectiveImplementation(
          CollectiveImplementationType::OneHalvingDoubling));
    } else {
      sys_panic(
          "Cannot interpret collective implementations. Please check the collective implementations in the sys"
          "input file");
    }
  }
  return result;
}
bool Sys::parse_var(std::string var, std::string value) {
  var = trim(var);
  value = trim(value);
  if (id == 0) {
    std::cout << "Var is: " << var << " ,val is: " << value << std::endl;
  }
  if (var == "scheduling-policy:") {
    inp_scheduling_policy = value;
  } else if (var == "all-reduce-implementation:") {
    std::stringstream mval(value);
    mval >> inp_all_reduce_implementation;
  } else if (var == "reduce-scatter-implementation:") {
    std::stringstream mval(value);
    mval >> inp_reduce_scatter_implementation;
  } else if (var == "all-gather-implementation:") {
    std::stringstream mval(value);
    mval >> inp_all_gather_implementation;
  } else if (var == "all-to-all-implementation:") {
    std::stringstream mval(value);
    mval >> inp_all_to_all_implementation;
  } else if (var == "collective-optimization:") {
    std::stringstream mval(value);
    mval >> inp_collective_optimization;
  } else if (var == "endpoint-delay:") {
    std::stringstream mval(value);
    mval >> communication_delay;
    communication_delay = communication_delay * injection_scale;
  } else if (var == "local-reduction-delay:") {
    std::stringstream mval(value);
    mval >> local_reduction_delay;
  } else if (var == "active-chunks-per-dimension:") {
    std::stringstream mval(value);
    mval >> active_chunks_per_dimension;
  } else if (var == "L:") {
    std::stringstream mval(value);
    mval >> inp_L;
  } else if (var == "o:") {
    std::stringstream mval(value);
    mval >> inp_o;
  } else if (var == "g:") {
    std::stringstream mval(value);
    mval >> inp_g;
  } else if (var == "G:") {
    std::stringstream mval(value);
    mval >> inp_G;
  } else if (var == "model-shared-bus:") {
    std::stringstream mval(value);
    mval >> inp_model_shared_bus;
  } else if (var == "preferred-dataset-splits:") {
    std::stringstream mval(value);
    mval >> preferred_dataset_splits;
  } else if (var == "boost-mode:") {
    std::stringstream mval(value);
    mval >> inp_boost_mode;
  } else if (var == "intra-dimension-scheduling:") {
    std::stringstream mval(value);
    std::string tmp;
    mval >> tmp;
    if (tmp == "FIFO") {
      intra_dimension_scheduling = IntraDimensionScheduling::FIFO;
    } else if (tmp == "RG") {
      intra_dimension_scheduling = IntraDimensionScheduling::RG;
    } else if (tmp == "SCF" || tmp == "smallestFirst") {
      // SCF scheduling is introduced in the ISCA 2022 paper: https://dl.acm.org/doi/abs/10.1145/3470496.3527382
      intra_dimension_scheduling = IntraDimensionScheduling::SmallestFirst;
    } else if (tmp == "lessRemainingPhaseFirst") {
      intra_dimension_scheduling =
          IntraDimensionScheduling::LessRemainingPhaseFirst;
    } else {
      sys_panic(
          "unknown value for intra-dimension-scheduling  in sys input file");
    }
  } else if (var == "inter-dimension-scheduling:") {
    std::stringstream mval(value);
    std::string tmp;
    mval >> tmp;
    if (tmp == "baseline" || tmp == "ascending") {
      inter_dimension_scheduling = InterDimensionScheduling::Ascending;
    } else if (tmp == "themis" || tmp == "offlineGreedy") {
      // Themis scheduling is introduced in the ISCA 2022 paper: https://dl.acm.org/doi/abs/10.1145/3470496.3527382
      inter_dimension_scheduling = InterDimensionScheduling::OfflineGreedy;
    } else if (tmp == "themisFlex" || tmp == "offlineGreedyFlex") {
      inter_dimension_scheduling = InterDimensionScheduling::OfflineGreedyFlex;
    } else if (tmp == "roundRobin") {
      inter_dimension_scheduling = InterDimensionScheduling::RoundRobin;
    } else {
      sys_panic(
          "unknown value for inter-dimension-scheduling  in sys input file");
    }
  } else if (var == "seprate-log:") {
    std::stringstream mval(value);
    int int_to_bool;
    mval >> int_to_bool;
    if (int_to_bool == 0) {
      this->seprate_log = false;
    } else {
      this->seprate_log = true;
    }
  } else if (var == "gap-denominator:") {
    std::stringstream mval(value);
    mval >> gap_denominator;
  } else if (var != "") {
    std::cerr
        << "######### Exiting because " << var
        << " is an unknown variable. Check your system input file. #########"
        << std::endl;
    exit(1);
  }
  return true;
}
bool Sys::post_process_inputs() {
  all_reduce_implementation_per_dimension =
      generate_collective_implementation_from_input(
          inp_all_reduce_implementation);
  if (all_reduce_implementation_per_dimension.size() == 0) {
    sys_panic("unknown value for all-reduce-implementation in sys input file");
  }
  reduce_scatter_implementation_per_dimension =
      generate_collective_implementation_from_input(
          inp_reduce_scatter_implementation);
  if (reduce_scatter_implementation_per_dimension.size() == 0) {
    sys_panic(
        "unknown value for reduce-scatter-implementation in sys input file");
  }
  all_gather_implementation_per_dimension =
      generate_collective_implementation_from_input(
          inp_all_gather_implementation);
  if (all_gather_implementation_per_dimension.size() == 0) {
    sys_panic("unknown value for all-gather-implementation in sys input file");
  }
  all_to_all_implementation_per_dimension =
      generate_collective_implementation_from_input(
          inp_all_to_all_implementation);
  if (all_to_all_implementation_per_dimension.size() == 0) {
    sys_panic("unknown value for all-to-all-implementation in sys input file");
  }
  if (inp_collective_optimization == "baseline") {
    collectiveOptimization = CollectiveOptimization::Baseline;
  } else if (inp_collective_optimization == "localBWAware") {
    collectiveOptimization = CollectiveOptimization::LocalBWAware;
  } else {
    sys_panic("unknown value for collective optimization in sys input file");
  }

  if (inp_boost_mode == 1) {
    boost_mode = true;
  } else {
    boost_mode = false;
  }
  if (inp_scheduling_policy == "LIFO") {
    this->scheduling_policy = SchedulingPolicy::LIFO;
  } else if (inp_scheduling_policy == "FIFO") {
    this->scheduling_policy = SchedulingPolicy::FIFO;
  } else {
    sys_panic("unknown value for scheduling policy in sys input file");
  }
  if (inp_model_shared_bus == 1) {
    model_shared_bus = true;
  } else {
    model_shared_bus = false;
  }
  return true;
}
bool Sys::initialize_sys(std::string name) {
  std::ifstream inFile;
  inFile.open(name);
  if (!inFile) {
    if (id == 0) {
      std::cerr << "Unable to open file: " << name << std::endl;
      std::cerr << "############ Exiting because unable to open the system "
                   "input file ############"
                << std::endl;
      std::cerr << "This error is fatal. Please check your path and filename."
                << std::endl;
    }
    exit(1);
  } else {
    if (id == 0) {
      std::cout << "Success in opening system file" << std::endl;
    }
  }
  std::string var;
  std::string value;
  while (inFile.peek() != EOF) {
    var = "";
    inFile >> var;
    if (inFile.peek() != EOF) {
      inFile >> value;
    }
    bool result = parse_var(var, value);
    if (result == false) {
      inFile.close();
      return result;
    }
  }
  inFile.close();
  return post_process_inputs();
}
Sys::SchedulerUnit::SchedulerUnit(
    Sys* sys,
    std::vector<int> queues,
    int max_running_streams,
    int ready_list_threshold,
    int queue_threshold) {
  this->sys = sys;
  this->ready_list_threshold = ready_list_threshold;
  this->queue_threshold = queue_threshold;
  this->max_running_streams = max_running_streams;

  this->latency_per_dimension.resize(queues.size(), 0);
  this->total_chunks_per_dimension.resize(queues.size(), 0);
  this->total_active_chunks_per_dimension.resize(queues.size(), 0);

  int base = 0;
  int dimension = 0;
  for (auto q : queues) {
    for (int i = 0; i < q; i++) {
      this->running_streams[base] = 0;
      std::list<BaseStream*>::iterator it;
      this->stream_pointer[base] = it;
      this->queue_id_to_dimension[base] = dimension;
      base++;
    }
    dimension++;
    UsageTracker u(2);
    usage.push_back(u);
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
void Sys::SchedulerUnit::notify_stream_added(int vnet) {
  if (sys->id == 0 &&
      ++total_active_chunks_per_dimension[queue_id_to_dimension[vnet]] == 1) {
    usage[queue_id_to_dimension[vnet]].increase_usage();
  }
  /*if(sys->id==0){
    std::cout<<"dim"<<queue_id_to_dimension[vnet]<<" chunk added at:
  "<<Sys::boostedTick()
             <<" ,current active: "<<
        total_active_chunks_per_dimension[queue_id_to_dimension[vnet]]<<std::endl;
  }*/
  stream_pointer[vnet] = sys->active_Streams[vnet].begin();
  std::advance(stream_pointer[vnet], running_streams[vnet]);
  while (stream_pointer[vnet] != sys->active_Streams[vnet].end() &&
         running_streams[vnet] < queue_threshold) {
    (*stream_pointer[vnet])->init();
    running_streams[vnet]++;
    std::advance(stream_pointer[vnet], 1);
  }
}
void Sys::SchedulerUnit::notify_stream_removed(int vnet, Tick running_time) {
  // std::cout<<"hello1, vnet: "<<vnet<<std::endl;
  if (sys->id == 0 &&
      --total_active_chunks_per_dimension[queue_id_to_dimension[vnet]] == 0) {
    usage[queue_id_to_dimension[vnet]].decrease_usage();
  }
  /*if(sys->id==0){
    std::cout<<"dim"<<queue_id_to_dimension[vnet]<<" chunk removed at:
  "<<Sys::boostedTick()
             <<" ,current active: "<<
             total_active_chunks_per_dimension[queue_id_to_dimension[vnet]]<<std::endl;
  }*/
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
    // sys->ask_for_schedule(max);
    sys->schedule(max);
  }
  // tmp
  stream_pointer[vnet] = sys->active_Streams[vnet].begin();
  std::advance(stream_pointer[vnet], running_streams[vnet]);
  while (stream_pointer[vnet] != sys->active_Streams[vnet].end() &&
         running_streams[vnet] < queue_threshold) {
    (*stream_pointer[vnet])->init();
    running_streams[vnet]++;
    std::advance(stream_pointer[vnet], 1);
  }
}
std::vector<double> Sys::SchedulerUnit::get_average_latency_per_dimension() {
  std::vector<double> result;
  result.resize(latency_per_dimension.size(), -1);
  for (int i = 0; i < result.size(); i++) {
    result[i] = latency_per_dimension[i] / total_chunks_per_dimension[i];
  }
  return result;
}
int Sys::nextPowerOf2(int n) {
  int count = 0;
  // First n in the below condition
  // is for the case where n is 0
  if (n && !(n & (n - 1)))
    return n;
  while (n != 0) {
    n >>= 1;
    count += 1;
  }
  return 1 << count;
}
void Sys::sys_panic(std::string msg) {
  std::cerr << msg << std::endl;
  exit(1);
}
void Sys::iterate() {
  call_events();
}
std::vector<std::string> Sys::split_string(std::string str, std::string sep) {
  char* cstr = const_cast<char*>(str.c_str());
  char* current;
  std::vector<std::string> arr;
  current = strtok(cstr, sep.c_str());
  while (current != nullptr) {
    arr.push_back(current);
    current = strtok(nullptr, sep.c_str());
  }
  return arr;
}
uint64_t Sys::determine_chunk_size(uint64_t size, ComType type) {
  uint64_t chunk_size = size / preferred_dataset_splits;
  return chunk_size;
}
DataSet* Sys::generate_all_reduce(
    uint64_t size,
    std::vector<bool> involved_dimensions,
    SchedulingPolicy pref_scheduling,
    int layer) {
  return generate_collective(
      size,
      layer,
      logical_topologies["AllReduce"],
      all_reduce_implementation_per_dimension,
      involved_dimensions,
      ComType::All_Reduce,
      pref_scheduling);
}
DataSet* Sys::generate_all_gather(
    uint64_t size,
    std::vector<bool> involved_dimensions,
    SchedulingPolicy pref_scheduling,
    int layer) {
  return generate_collective(
      size,
      layer,
      logical_topologies["AllGather"],
      all_gather_implementation_per_dimension,
      involved_dimensions,
      ComType::All_Gatehr,
      pref_scheduling);
}
DataSet* Sys::generate_reduce_scatter(
    uint64_t size,
    std::vector<bool> involved_dimensions,
    SchedulingPolicy pref_scheduling,
    int layer) {
  return generate_collective(
      size,
      layer,
      logical_topologies["ReduceScatter"],
      reduce_scatter_implementation_per_dimension,
      involved_dimensions,
      ComType::Reduce_Scatter,
      pref_scheduling);
}
DataSet* Sys::generate_all_to_all(
    uint64_t size,
    std::vector<bool> involved_dimensions,
    SchedulingPolicy pref_scheduling,
    int layer) {
  return generate_collective(
      size,
      layer,
      logical_topologies["AllToAll"],
      all_to_all_implementation_per_dimension,
      involved_dimensions,
      ComType::All_to_All,
      pref_scheduling);
}
CollectivePhase Sys::generate_collective_phase(
    ComType collective_type,
    int layer_num,
    BasicLogicalTopology* topology,
    uint64_t data_size,
    int queue_id,
    RingTopology::Direction direction,
    InjectionPolicy injection_policy,
    CollectiveImplementation* collective_implementation,
    bool boost_mode) {
  if (collective_implementation->type == CollectiveImplementationType::Ring ||
      collective_implementation->type ==
          CollectiveImplementationType::OneRing) {
    CollectivePhase vn(
        this,
        queue_id,
        new Ring(
            collective_type,
            id,
            layer_num,
            (RingTopology*)topology,
            data_size,
            direction,
            injection_policy,
            boost_mode));
    return vn;
  } else if (
      collective_implementation->type == CollectiveImplementationType::Direct ||
      collective_implementation->type ==
          CollectiveImplementationType::OneDirect) {
    CollectivePhase vn(
        this,
        queue_id,
        new AllToAll(
            collective_type,
            ((DirectCollectiveImplementation*)collective_implementation)
                ->direct_collective_window,
            id,
            layer_num,
            (RingTopology*)topology,
            data_size,
            direction,
            InjectionPolicy::Normal,
            boost_mode));
    return vn;
  } else if (
      collective_implementation->type ==
      CollectiveImplementationType::DoubleBinaryTree) {
    CollectivePhase vn(
        this,
        queue_id,
        new DoubleBinaryTreeAllReduce(
            id, layer_num, (BinaryTree*)topology, data_size, boost_mode));
    return vn;
  } else if (
      collective_implementation->type ==
          CollectiveImplementationType::HalvingDoubling ||
      collective_implementation->type ==
          CollectiveImplementationType::OneHalvingDoubling) {
    CollectivePhase vn(
        this,
        queue_id,
        new HalvingDoubling(
            collective_type,
            id,
            layer_num,
            (RingTopology*)topology,
            data_size,
            boost_mode));
    return vn;
  } else {
    std::cerr
        << "Error: No known collective implementation for collective phase"
        << std::endl;
    exit(1);
  }
}
DataSet* Sys::generate_collective(
    uint64_t size,
    int layer_num,
    LogicalTopology* topology,
    std::vector<CollectiveImplementation*> implementation_per_dimension,
    std::vector<bool> dimensions_involved,
    ComType collective_type,
    SchedulingPolicy pref_scheduling) {
  uint64_t chunk_size = determine_chunk_size(size, collective_type);
  uint64_t recommended_chunk_size = chunk_size;
  int streams = ceil(((double)size) / chunk_size);
  int tmp;
  DataSet* dataset = new DataSet(streams);
  int pri = get_priority(pref_scheduling);
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
    chunk_size=std::min(chunk_size,size); // checking for underflow in corner cases
    std::vector<int> dim_mapper(topology->get_num_of_dimensions());
    std::iota(std::begin(dim_mapper), std::end(dim_mapper), 0);
    if (collective_type == ComType::All_Gatehr) {
      std::reverse(dim_mapper.begin(), dim_mapper.end());
    }

    if (inter_dimension_scheduling == InterDimensionScheduling::RoundRobin) {
      std::rotate(
          dim_mapper.begin(),
          dim_mapper.begin() + round_robin_inter_dimension_scheduler,
          dim_mapper.end());
      // std::rotate(dimensions_involved.begin(),dimensions_involved.begin()+round_robin_inter_dimension_scheduler,
      // dimensions_involved.begin()+topology->get_num_of_dimensions());
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
          stream_counter,
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
      size -= chunk_size;
    }
    tmp = chunk_size;
    std::list<CollectivePhase> vect;

    if (collective_type != ComType::All_Reduce ||
        collectiveOptimization == CollectiveOptimization::Baseline) {
      for (int dim = 0; dim < topology->get_num_of_dimensions(); dim++) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1 ||
            !dimensions_involved[dim_mapper[dim]]) {
          continue;
        }
        std::pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            collective_type,
            layer_num,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], collective_type),
            tmp,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]],
            boost_mode);
        vect.push_back(phase);
        tmp = phase.final_data_size;
      }
    } else if (
        inter_dimension_scheduling == InterDimensionScheduling::OfflineGreedy ||
        inter_dimension_scheduling ==
            InterDimensionScheduling::OfflineGreedyFlex ||
        inter_dimension_scheduling == InterDimensionScheduling::OnlineGreedy) {
      int dim = 0;
      for (dim = 0; dim < topology->get_num_of_dimensions(); dim++) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1 ||
            !dimensions_involved[dim_mapper[dim]]) {
          continue;
        }
        std::pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            ComType::Reduce_Scatter,
            layer_num,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], ComType::Reduce_Scatter),
            tmp,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]],
            boost_mode);
        vect.push_back(phase);
        tmp = phase.final_data_size;
      }
      dim--;
      for (; dim >= 0; dim--) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1 ||
            !dimensions_involved[dim_mapper[dim]]) {
          continue;
        }
        std::pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            ComType::All_Gatehr,
            layer_num,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], ComType::All_Gatehr),
            tmp,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]],
            boost_mode);
        vect.push_back(phase);
        tmp = phase.final_data_size;
      }
    } else {
      int dim = 0;
      int last_active_dim = 0;
      for (dim = 0; dim < topology->get_num_of_dimensions(); dim++) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) != 1 &&
            dimensions_involved[dim_mapper[dim]]) {
          last_active_dim = dim;
        }
      }
      for (dim = 0; dim < last_active_dim; dim++) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1 ||
            !dimensions_involved[dim_mapper[dim]]) {
          continue;
        }
        std::pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            ComType::Reduce_Scatter,
            layer_num,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], ComType::Reduce_Scatter),
            tmp,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]],
            boost_mode);
        vect.push_back(phase);
        tmp = phase.final_data_size;
      }
      while (dim > 0 &&
             (dimensions_involved[dim_mapper[dim]] == false ||
              topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1)) {
        dim--;
      }
      if (dimensions_involved[dim_mapper[dim]] &&
          topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) > 1) {
        std::pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            ComType::All_Reduce,
            layer_num,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], ComType::All_Reduce),
            tmp,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]],
            boost_mode);
        vect.push_back(phase);
        tmp = phase.final_data_size;
      }
      dim--;
      for (; dim >= 0; dim--) {
        if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) == 1 ||
            !dimensions_involved[dim_mapper[dim]]) {
          continue;
        }
        std::pair<int, RingTopology::Direction> queue =
            vLevels->get_next_queue_at_level(dim_mapper[dim]);
        CollectivePhase phase = generate_collective_phase(
            ComType::All_Gatehr,
            layer_num,
            topology->get_basic_topology_at_dimension(
                dim_mapper[dim], ComType::All_Gatehr),
            tmp,
            queue.first,
            queue.second,
            InjectionPolicy::Normal,
            implementation_per_dimension[dim_mapper[dim]],
            boost_mode);
        vect.push_back(phase);
        tmp = phase.final_data_size;
      }
    }
    if (vect.size() > 0) {
      StreamBaseline* newStream =
          new StreamBaseline(this, dataset, stream_counter++, vect, pri);
      newStream->current_queue_id = -1;
      insert_into_ready_list(newStream);
    } else {
      dataset->active = false;
      break;
    }
  }
  if (dataset->active) {
    streams_injected += count;
    dataset->total_streams = count;
  }
  return dataset;
}
void Sys::call_events() {
  for (auto& callable : event_queue[Sys::boostedTick()]) {
    try {
      pending_events--;
      (std::get<0>(callable))
          ->call(std::get<1>(callable), std::get<2>(callable));
    } catch (...) {
      std::cerr << "warning! a callable is removed before call" << std::endl;
    }
  }
  if (event_queue[Sys::boostedTick()].size() > 0) {
    event_queue[Sys::boostedTick()].clear();
  }
  event_queue.erase(Sys::boostedTick());
  if ((finished_workloads == 1 && event_queue.size() == 0) ||
      initialized == false) {
    delete this;
  }
}
void Sys::exitSimLoop(std::string msg) {
  std::cout << msg << std::endl;
  NI->sim_finish();
  return;
}
Tick Sys::boostedTick() {
  Sys* ts = all_generators[0];
  if (ts == nullptr) {
    for (int i = 1; i < all_generators.size(); i++) {
      if (all_generators[i] != nullptr) {
        ts = all_generators[i];
        break;
      }
    }
  }
  timespec_t tmp = ts->NI->sim_get_time();
  Tick tick = tmp.time_val / CLOCK_PERIOD;
  return tick + offset;
}
void Sys::proceed_to_next_vnet_baseline(StreamBaseline* stream) {
  // int added_delay=0;
  /*if (id == 0) {
    std::cout << "stream: " << stream->stream_num
              << "  scheduled after finishd steps: " << stream->steps_finished
              << " in  node:" << id
              << " ,remaining: " << stream->phases_to_go.size()
              << " ,at time: " << boostedTick()
              << " ,initial data size: " << stream->initial_data_size
              << " ,available synchronizer: "
              << stream->synchronizer[stream->stream_num] << std::endl;
  }*/
  if (!stream->is_ready()) {
    stream->suspend_ready();
    return;
  }
  stream->consume_ready();
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
  // std::cout<<"here we are 2.5"<<std::endl;
  if (stream->phases_to_go.size() == 0) {
    stream->take_bus_stats_average();
    stream->dataset->notify_stream_finished((StreamStat*)stream);
    /*if (id == 0) {
      std::cout << "stream number: " << stream->stream_num
                << "  finished its execution in time: " << Sys::boostedTick()
                << " total injected: " << streams_injected
                << " ,total finished: " << streams_finished
                << " ,total running streams: " << total_running_streams
                << " ,pri: " << stream->priority << std::endl;
    }*/
  }
  if (stream->current_queue_id >= 0 && stream->my_current_phase.enabled) {
    std::list<BaseStream*>& target =
        active_Streams.at(stream->my_current_phase.queue_id);
    for (std::list<BaseStream*>::iterator it = target.begin();
         it != target.end();
         ++it) {
      if (((StreamBaseline*)(*it))->stream_num == stream->stream_num) {
        // std::cout<<"deleted from scheduler"<<std::endl;
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

  /*if (id == 0) {
    std::cout << "info ,for node: " << id << " stream num "
              << stream->stream_num
              << " has been changed to vnet: " << stream->current_queue_id
              << " at time: " << boostedTick()
              << " ,remaining: " << stream->phases_to_go.size()
              << " ,intial size: " << stream->initial_data_size
              << " total injected: " << streams_injected
              << " ,total finished: " << streams_finished
              << " ,total running streams: " << total_running_streams
              << " ,pri: " << stream->priority << std::endl;
  }*/
  if (stream->my_current_phase.enabled) {
    insert_stream(&active_Streams[stream->current_queue_id], stream);
  }

  stream->state = StreamState::Ready;

  if (!stream->my_current_phase.enabled) {
    stream->declare_ready();
    stream->suspend_ready();
  }
  if (previous_vnet >= 0) {
    scheduler_unit->notify_stream_removed(
        previous_vnet, Sys::boostedTick() - stream->last_init);
  }
  scheduler_unit->notify_stream_added(stream->current_queue_id);
  /*if(!stream->my_current_phase.enabled){
      std::cout<<"I am node: "<<id<<"and the current chunk:
  "<<stream->stream_num
              <<"is disabled on me for phase
  "<<stream->steps_finished<<std::endl;
  }*/
}
void Sys::exiting() {}
void Sys::insert_stream(std::list<BaseStream*>* queue, BaseStream* baseStream) {
  std::list<BaseStream*>::iterator it = queue->begin();
  if (intra_dimension_scheduling == IntraDimensionScheduling::FIFO ||
      baseStream->current_queue_id < 0 ||
      baseStream->current_com_type == ComType::All_to_All ||
      baseStream->current_com_type == ComType::All_Reduce) {
    while (it != queue->end()) {
      if ((*it)->initialized == true) {
        std::advance(it, 1);
        continue;
      } else if ((*it)->priority >= baseStream->priority) {
        std::advance(it, 1);
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
        std::advance(it, 1);
        if (it != queue->end() && (*it)->initialized == false) {
          one_to_last = last;
          last = (*it)->current_com_type;
          std::advance(it, 1);
        }
        continue;
      } else if ((*it)->priority > baseStream->priority) {
        std::advance(it, 1);
        continue;
      } else if (
          (last == ComType::Reduce_Scatter &&
           one_to_last == ComType::All_Gatehr) ||
          (last == ComType::All_Gatehr &&
           one_to_last == ComType::Reduce_Scatter)) {
        std::advance(it, 1);
        continue;
      } else {
        break;
      }
    }
  } else if (
      intra_dimension_scheduling == IntraDimensionScheduling::SmallestFirst) {
    while (it != queue->end()) {
      if ((*it)->initialized == true) {
        std::advance(it, 1);
        continue;
      } else if (
          (*it)->my_current_phase.initial_data_size <
          baseStream->my_current_phase.initial_data_size) {
        std::advance(it, 1);
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
        std::advance(it, 1);
        continue;
      } else if ((*it)->phases_to_go.size() < baseStream->phases_to_go.size()) {
        std::advance(it, 1);
        continue;
      } else {
        break;
      }
    }
  }
  queue->insert(it, baseStream);
}
void Sys::register_for_finished_stream(Callable* callable) {
  registered_for_finished_stream_event.push_back(callable);
}
void Sys::increase_finished_streams(int amount) {
  streams_finished += amount;
  for (auto c : registered_for_finished_stream_event) {
    c->call(EventType::StreamsFinishedIncrease, nullptr);
  }
}

void Sys::register_phases(
    BaseStream* stream,
    std::list<CollectivePhase> phases_to_go) {
  for (auto& vnet : phases_to_go) {
    stream_priorities[vnet.queue_id].push_back(stream->stream_num);
  }
}

void Sys::register_event(
    Callable* callable,
    EventType event,
    CallData* callData,
    int cycles) {
  Tick mycycles = cycles;
  try_register_event(callable, event, callData, mycycles);
  return;
}
void Sys::call(EventType type, CallData* data) {
  if (id == 0 && type == EventType::General) {
    increase_finished_streams(1);
  }
}
void Sys::try_register_event(
    Callable* callable,
    EventType event,
    CallData* callData,
    Tick& cycles) {
  bool should_schedule = false;
  if (event_queue.find(Sys::boostedTick() + cycles) == event_queue.end()) {
    std::list<std::tuple<Callable*, EventType, CallData*>> tmp;
    event_queue[Sys::boostedTick() + cycles] = tmp;
    should_schedule = true;
  }
  event_queue[Sys::boostedTick() + cycles].push_back(
      std::make_tuple(callable, event, callData));
  if (should_schedule) {
    timespec_t tmp = generate_time(cycles);
    BasicEventHandlerData* data =
        new BasicEventHandlerData(id, EventType::CallEvents);
    NI->sim_schedule(tmp, &Sys::handleEvent, data);
  }
  cycles = 0;
  pending_events++;
  return;
}
void Sys::insert_into_ready_list(BaseStream* stream) {
  insert_stream(&ready_list, stream);
  scheduler_unit->notify_stream_added_into_ready_list();
}
void Sys::ask_for_schedule(int max) {
  if (ready_list.size() == 0 ||
      ready_list.front()->synchronizer[ready_list.front()->stream_num] <
          all_generators.size()) {
    return;
  }
  int top = ready_list.front()->stream_num;
  int min = ready_list.size();
  if (min > max) {
    min = max;
  }
  // std::cout<<"ask for schedule checkpoint 1, all gen size:
  // "<<all_generators.size()<<std::endl;
  for (auto& gen : all_generators) {
    if (gen->ready_list.size() == 0 ||
        gen->ready_list.front()->stream_num != top) {
      // std::cout<<"ask for schedule checkpoint 2.1"<<std::endl;
      return;
    }
    if (gen->ready_list.size() < min) {
      // std::cout<<"ask for schedule checkpoint 2.2"<<std::endl;
      min = gen->ready_list.size();
    }
  }
  // std::cout<<"ask for schedule checkpoint 3"<<std::endl;
  for (auto& gen : all_generators) {
    gen->schedule(min);
  }
  return;
}
void Sys::schedule(int num) {
  int ready_list_size = ready_list.size();
  int counter = std::min(num, ready_list_size);
  // std::cout<<"schedule is called with num: "<<num<<" and counter"<<std::endl;
  while (counter > 0) {
    // register_phases(ready_list.front(),ready_list.front()->phases_to_go);
    int top_vn = ready_list.front()->phases_to_go.front().queue_id;
    int total_waiting_streams = ready_list.size();
    int total_phases = ready_list.front()->phases_to_go.size();

    proceed_to_next_vnet_baseline((StreamBaseline*)ready_list.front());

    if (ready_list.front()->current_queue_id == -1) {
      Sys::sys_panic(
          "should not happen! " +
          std::to_string(
              BaseStream::synchronizer[ready_list.front()->stream_num]) +
          " , " +
          std::to_string(
              BaseStream::ready_counter[ready_list.front()->stream_num]) +
          " , top queue id: " + std::to_string(top_vn) +
          " , total phases: " + std::to_string(total_phases) +
          " , waiting streams: " + std::to_string(total_waiting_streams));
    }
    ready_list.pop_front();
    counter--;
    first_phase_streams++;
    total_running_streams++;
  }
}
void Sys::handleEvent(void* arg) {
  if (arg == nullptr) {
    return;
  }
  BasicEventHandlerData* ehd = (BasicEventHandlerData*)arg;
  int id = ehd->nodeId;
  EventType event = ehd->event;

  if (event == EventType::CallEvents) {
    // std::cout<<"handle event triggered at node: "<<id<<" for call events! at
    // time: "<<Sys::boostedTick()<<std::endl;
    all_generators[id]->iterate();
    delete ehd;
  } else if (event == EventType::RendezvousSend) {
    // std::cout<<"rendevouz send handle event triggered at node: "<<id<<" for
    // call events! at time: "<<Sys::boostedTick()<<std::endl;
    RendezvousSendData* rsd = (RendezvousSendData*)ehd;
    rsd->send->call(EventType::General, nullptr);
    delete rsd;
  } else if (event == EventType::RendezvousRecv) {
    // std::cout<<"rendevouz recv triggered at node: "<<id<<" for call events!
    // at time: "<<Sys::boostedTick()<<std::endl;
    RendezvousRecvData* rrd = (RendezvousRecvData*)ehd;
    rrd->recv->call(EventType::General, nullptr);
    delete rrd;
  } else if (event == EventType::PacketReceived) {
    RecvPacketEventHadndlerData* rcehd = (RecvPacketEventHadndlerData*)ehd;
    // std::cout<<"****************************handle event triggered for
    // received packets! at node: "
    //<<rcehd->owner->owner->id<<" at time: "<<Sys::boostedTick()<<" ,Tag:
    //"<<rcehd->owner->stream_num<<std::endl;
    rcehd->owner->consume(rcehd);
    delete rcehd;
  }
}
timespec_t Sys::generate_time(int cycles) {
  timespec_t tmp = NI->sim_get_time();
  double addition = cycles * ((double)CLOCK_PERIOD);
  tmp.time_val = addition;
  return tmp;
}
} // namespace AstraSim
