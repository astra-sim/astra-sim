// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/SimpleSys.hh>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>

#include <astra-sim/system/common/BasicEventHandlerData.hh>
#include <astra-sim/system/common/WorkloadLayerHandlerData.hh>
#include <astra-sim/system/simple/BaseStream.hh>
#include <astra-sim/system/simple/CollectivePlan.hh>
#include <astra-sim/system/simple/CommunicatorGroup.hh>
#include <astra-sim/system/simple/DataSet.hh>
#include <astra-sim/system/simple/MemBus.hh>
#include <astra-sim/system/simple/QueueLevels.hh>
#include <astra-sim/system/simple/RecvPacketEventHandlerData.hh>
#include <astra-sim/system/simple/StreamBaseline.hh>
#include <astra-sim/system/simple/astraccl/Algorithm.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/collective_algorithm/AllToAll.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/collective_algorithm/DoubleBinaryTreeAllReduce.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/collective_algorithm/HalvingDoubling.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/collective_algorithm/Ring.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/BasicLogicalTopology.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/GeneralComplexTopology.hh>
#include <astra-sim/system/simple/scheduling/OfflineGreedy.hh>
#include <astra-sim/workload/Workload.hh>

#include <feeder/wrapper_node.h>
#include <json/json.hpp>

using json = nlohmann::json;

typedef JSONNode::CollectiveCommType ChakraCollectiveCommType;

namespace AstraSim {

// SchedulerUnit --------------------------------------------------------------
SimpleSys::SchedulerUnit::SchedulerUnit(SimpleSys* sys,
                                        std::vector<int> queues,
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

void SimpleSys::SchedulerUnit::notify_stream_added(int vnet) {
    if (sys->id == 0 &&
        ++total_active_chunks_per_dimension[queue_id_to_dimension[vnet]] == 1) {
        usage[queue_id_to_dimension[vnet]].increase_usage();
    }
    stream_pointer[vnet] = sys->active_Streams[vnet].begin();
    std::advance(stream_pointer[vnet], running_streams[vnet]);
    while (stream_pointer[vnet] != sys->active_Streams[vnet].end() &&
           running_streams[vnet] < queue_threshold) {
        (*stream_pointer[vnet])->init();
        running_streams[vnet]++;
        std::advance(stream_pointer[vnet], 1);
    }
}

void SimpleSys::SchedulerUnit::notify_stream_added_into_ready_list() {
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

void SimpleSys::SchedulerUnit::notify_stream_removed(int vnet,
                                                     Tick running_time) {
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
    std::advance(stream_pointer[vnet], running_streams[vnet]);
    while (stream_pointer[vnet] != sys->active_Streams[vnet].end() &&
           running_streams[vnet] < queue_threshold) {
        (*stream_pointer[vnet])->init();
        running_streams[vnet]++;
        std::advance(stream_pointer[vnet], 1);
    }
}

//-----------------------------------------------------------------------------

/** Builds a Sys that runs native collectives over logical topologies. */
SimpleSys::SimpleSys(int id,
                     std::string workload_configuration,
                     std::string comm_group_configuration,
                     std::string system_configuration,
                     AstraRemoteMemoryAPI* remote_mem,
                     AstraNetworkAPI* comm_NI,
                     std::vector<int> physical_dims,
                     std::vector<int> queues_per_dim,
                     double injection_scale,
                     [[maybe_unused]] double comm_scale,
                     [[maybe_unused]] bool rendezvous_enabled,
                     uint64_t comm_size,
                     std::string custom_comm_plan_map_filename)
    : Sys(id,
          workload_configuration,
          comm_group_configuration,
          system_configuration,
          remote_mem,
          comm_NI,
          comm_size,
          custom_comm_plan_map_filename) {
    this->memBus = nullptr;
    this->inp_L = 0;
    this->inp_o = 0;
    this->inp_g = 0;
    this->inp_G = 0;
    this->model_shared_bus = 0;
    this->injection_scale = injection_scale;
    this->communication_delay = 0;
    this->local_reduction_delay = 0;

    this->scheduler_unit = nullptr;
    this->vLevels = nullptr;
    this->offline_greedy = nullptr;
    this->intra_dimension_scheduling = IntraDimensionScheduling::FIFO;
    this->inter_dimension_scheduling = InterDimensionScheduling::Ascending;
    this->round_robin_inter_dimension_scheduler = 0;
    this->active_chunks_per_dimension = 1;
    this->priority_counter = 0;
    this->preferred_dataset_splits = 0;
    this->scheduling_policy = SchedulingPolicy::LIFO;
    this->collectiveOptimization = CollectiveOptimization::Baseline;

    this->last_scheduled_collective = 0;

    this->first_phase_streams = 0;
    this->total_running_streams = 0;

    this->communication_delay = 10000;
    this->local_reduction_delay = 1000;

    parseSimpleConfig(system_configuration);

    // scheduler
    this->physical_dims = physical_dims;

    // Total NPU count: the product of the physical dimension sizes
    this->total_nodes = 1;
    for (uint64_t current_dim = 0; current_dim < queues_per_dim.size();
         current_dim++) {
        if (physical_dims[current_dim] >= 1) {
            this->total_nodes *= physical_dims[current_dim];
        }
    }

    int element = 0;
    this->dim_to_break = -1;
    for (uint64_t current_dim = 0; current_dim < queues_per_dim.size();
         current_dim++) {
        for (int j = 0; j < queues_per_dim[current_dim]; j++) {
            std::list<BaseStream*> temp;
            active_Streams[element] = temp;
            element++;
        }
    }

    this->concurrent_streams =
        (int)ceil(((double)active_chunks_per_dimension) / queues_per_dim[0]);
    this->active_first_phase = 100000000;
    this->max_running = 100000000;

    scheduler_unit = new SchedulerUnit(this, queues_per_dim, max_running,
                                       active_first_phase, concurrent_streams);

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

    memBus = new MemBus("NPU", "MA", this, inp_L, inp_o, inp_g, inp_G,
                        model_shared_bus, communication_delay, true);

    if (inter_dimension_scheduling == InterDimensionScheduling::OfflineGreedy ||
        inter_dimension_scheduling ==
            InterDimensionScheduling::OfflineGreedyFlex) {
        offline_greedy = new OfflineGreedy(this);
    }
}

SimpleSys::~SimpleSys() {
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

    if (scheduler_unit != nullptr) {
        delete scheduler_unit;
    }

    if (vLevels != nullptr) {
        delete vLevels;
    }

    if (memBus != nullptr) {
        delete memBus;
    }

    if (offline_greedy != nullptr) {
        delete offline_greedy;
    }
}

void SimpleSys::parseSimpleConfig(const std::string& filename) {
    std::ifstream inFile;
    inFile.open(filename);
    if (!inFile) {
        if (id == 0) {
            std::cerr << "Unable to open file: " << filename << std::endl;
        }
        exit(1);
    }

    json j;
    inFile >> j;

    if (j.contains("scheduling-policy")) {
        std::string inp_scheduling_policy = j["scheduling-policy"];
        if (inp_scheduling_policy == "LIFO") {
            this->scheduling_policy = SchedulingPolicy::LIFO;
        } else if (inp_scheduling_policy == "FIFO") {
            this->scheduling_policy = SchedulingPolicy::FIFO;
        } else if (inp_scheduling_policy == "EXPLICIT") {
            this->scheduling_policy = SchedulingPolicy::EXPLICIT;
        } else {
            sysPanic("unknown value for scheduling policy in sys input file");
        }
    }
    if (j.contains("all-reduce-implementation")) {
        std::vector<std::string> collective_impl_str_vec =
            j["all-reduce-implementation"];
        for (auto collective_impl_str : collective_impl_str_vec) {
            CollectiveImpl* ci =
                generate_collective_impl_from_input(collective_impl_str);
            all_reduce_implementation_per_dimension.push_back(ci);
        }
    }
    if (j.contains("reduce-scatter-implementation")) {
        std::vector<std::string> collective_impl_str_vec =
            j["reduce-scatter-implementation"];
        for (auto collective_impl_str : collective_impl_str_vec) {
            CollectiveImpl* ci =
                generate_collective_impl_from_input(collective_impl_str);
            reduce_scatter_implementation_per_dimension.push_back(ci);
        }
    }
    if (j.contains("all-gather-implementation")) {
        std::vector<std::string> collective_impl_str_vec =
            j["all-gather-implementation"];
        for (auto collective_impl_str : collective_impl_str_vec) {
            CollectiveImpl* ci =
                generate_collective_impl_from_input(collective_impl_str);
            all_gather_implementation_per_dimension.push_back(ci);
        }
    }
    if (j.contains("all-to-all-implementation")) {
        std::vector<std::string> collective_impl_str_vec =
            j["all-to-all-implementation"];
        for (auto collective_impl_str : collective_impl_str_vec) {
            CollectiveImpl* ci =
                generate_collective_impl_from_input(collective_impl_str);
            all_to_all_implementation_per_dimension.push_back(ci);
        }
    }
    if (j.contains("collective-optimization")) {
        std::string inp_collective_optimization = j["collective-optimization"];
        if (inp_collective_optimization == "baseline") {
            collectiveOptimization = CollectiveOptimization::Baseline;
        } else if (inp_collective_optimization == "localBWAware") {
            collectiveOptimization = CollectiveOptimization::LocalBWAware;
        } else {
            sysPanic(
                "unknown value for collective optimization in sys input file");
        }
    }
    if (j.contains("local-reduction-delay")) {
        // Config values are given in ns; scale to ps to match the
        // already-ps-scaled hard-coded default (was read raw, making a
        // configured value 1000x too small relative to the default).
        local_reduction_delay = j["local-reduction-delay"];
        local_reduction_delay = local_reduction_delay * 1000;
    }
    if (j.contains("active-chunks-per-dimension")) {
        active_chunks_per_dimension = j["active-chunks-per-dimension"];
    }
    if (j.contains("L")) {
        inp_L = j["L"];
        inp_L = inp_L * 1000;  // ns -> ps
    }
    if (j.contains("o")) {
        inp_o = j["o"];
        inp_o = inp_o * 1000;  // ns -> ps
    }
    if (j.contains("g")) {
        inp_g = j["g"];
        inp_g = inp_g * 1000;  // ns -> ps
    }
    if (j.contains("G")) {
        inp_G = j["G"];
        inp_G = inp_G * 1000;  // ns -> ps
    }
    if (j.contains("endpoint-delay")) {
        // Config value is given in ns; scale to ps to match the
        // already-ps-scaled hard-coded default (was read raw, making a
        // configured value 1000x too small relative to the default).
        communication_delay = j["endpoint-delay"];
        communication_delay = communication_delay * 1000;
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

    inFile.close();
}

CollectiveImpl* SimpleSys::generate_collective_impl_from_input(
    std::string collective_impl_str) {
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
        sysPanic("Cannot interpret collective implementations. Please check "
                 "the collective implementations in the sys"
                 "input file");
        return new CollectiveImpl(CollectiveImplType::Ring);
    }
}

LogicalTopology* SimpleSys::get_logical_topology(ComType comm_type) {
    if (comm_type == ComType::All_Reduce) {
        return logical_topologies["AllReduce"];
    } else if (comm_type == ComType::All_to_All) {
        return logical_topologies["AllToAll"];
    } else if (comm_type == ComType::Reduce_Scatter) {
        return logical_topologies["ReduceScatter"];
    } else if (comm_type == ComType::All_Gather) {
        return logical_topologies["AllGather"];
    } else {
        sysPanic("no known logical topology!");
        return nullptr;
    }
}

std::vector<CollectiveImpl*> SimpleSys::get_collective_implementation(
    ComType comm_type) {
    if (comm_type == ComType::All_Reduce) {
        return all_reduce_implementation_per_dimension;
    } else if (comm_type == ComType::All_to_All) {
        return all_to_all_implementation_per_dimension;
    } else if (comm_type == ComType::Reduce_Scatter) {
        return reduce_scatter_implementation_per_dimension;
    } else if (comm_type == ComType::All_Gather) {
        return all_gather_implementation_per_dimension;
    } else {
        sysPanic("no known collective implementation!");
        std::vector<CollectiveImpl*> tmp;
        return tmp;
    }
}

DataSet* SimpleSys::generate_all_reduce(uint64_t size,
                                        std::vector<bool> involved_dimensions,
                                        CommunicatorGroup* communicator_group,
                                        int explicit_priority) {
    if (communicator_group == nullptr) {
        return generate_collective(size, logical_topologies["AllReduce"],
                                   all_reduce_implementation_per_dimension,
                                   involved_dimensions, ComType::All_Reduce,
                                   explicit_priority, communicator_group);
    } else {
        CollectivePlan* plan =
            communicator_group->get_collective_plan(ComType::All_Reduce);
        return generate_collective(
            size, plan->topology, plan->implementation_per_dimension,
            plan->dimensions_involved, ComType::All_Reduce, explicit_priority,
            communicator_group);
    }
}

DataSet* SimpleSys::generate_all_to_all(uint64_t size,
                                        std::vector<bool> involved_dimensions,
                                        CommunicatorGroup* communicator_group,
                                        int explicit_priority) {
    if (communicator_group == nullptr) {
        return generate_collective(size, logical_topologies["AllToAll"],
                                   all_to_all_implementation_per_dimension,
                                   involved_dimensions, ComType::All_to_All,
                                   explicit_priority, communicator_group);
    } else {
        CollectivePlan* plan =
            communicator_group->get_collective_plan(ComType::All_to_All);
        return generate_collective(
            size, plan->topology, plan->implementation_per_dimension,
            plan->dimensions_involved, ComType::All_to_All, explicit_priority,
            communicator_group);
    }
}

DataSet* SimpleSys::generate_all_gather(uint64_t size,
                                        std::vector<bool> involved_dimensions,
                                        CommunicatorGroup* communicator_group,
                                        int explicit_priority) {
    if (communicator_group == nullptr) {
        return generate_collective(size, logical_topologies["AllGather"],
                                   all_gather_implementation_per_dimension,
                                   involved_dimensions, ComType::All_Gather,
                                   explicit_priority, communicator_group);
    } else {
        CollectivePlan* plan =
            communicator_group->get_collective_plan(ComType::All_Gather);
        return generate_collective(
            size, plan->topology, plan->implementation_per_dimension,
            plan->dimensions_involved, ComType::All_Gather, explicit_priority,
            communicator_group);
    }
}

DataSet* SimpleSys::generate_reduce_scatter(
    uint64_t size,
    std::vector<bool> involved_dimensions,
    CommunicatorGroup* communicator_group,
    int explicit_priority) {
    if (communicator_group == nullptr) {
        return generate_collective(size, logical_topologies["ReduceScatter"],
                                   reduce_scatter_implementation_per_dimension,
                                   involved_dimensions, ComType::Reduce_Scatter,
                                   explicit_priority, communicator_group);
    } else {
        CollectivePlan* plan =
            communicator_group->get_collective_plan(ComType::Reduce_Scatter);
        return generate_collective(
            size, plan->topology, plan->implementation_per_dimension,
            plan->dimensions_involved, ComType::Reduce_Scatter,
            explicit_priority, communicator_group);
    }
}

DataSet* SimpleSys::generate_collective(
    uint64_t size,
    LogicalTopology* topology,
    std::vector<CollectiveImpl*> implementation_per_dimension,
    std::vector<bool> dimensions_involved,
    ComType collective_type,
    int explicit_priority,
    CommunicatorGroup* communicator_group) {
    uint64_t chunk_size = determine_chunk_size(size, collective_type);

    // if LDST based implementation, 1 stream for now, overridding chunk size
    if (communicator_group && communicator_group->isLDSTImpl()) {
        chunk_size = size;
    }

    uint64_t recommended_chunk_size = chunk_size;
    int streams = ceil(((double)size) / chunk_size);
    uint64_t remain_size;
    DataSet* dataset = new DataSet(streams);
    int pri = get_priority(explicit_priority);
    int count = 0;
    if (id == 0 && (inter_dimension_scheduling ==
                        InterDimensionScheduling::OfflineGreedy ||
                    inter_dimension_scheduling ==
                        InterDimensionScheduling::OfflineGreedyFlex)) {
        if (last_scheduled_collective != Sys::boostedTick()) {
            offline_greedy->reset_loads();
            last_scheduled_collective = Sys::boostedTick();
        }
    }

    while (size > 0) {
        count++;

        std::vector<int> dim_mapper(topology->get_num_of_dimensions());
        iota(begin(dim_mapper), end(dim_mapper), 0);
        if (collective_type == ComType::All_Gather) {
            reverse(dim_mapper.begin(), dim_mapper.end());
        }

        if (inter_dimension_scheduling ==
            InterDimensionScheduling::RoundRobin) {
            rotate(dim_mapper.begin(),
                   dim_mapper.begin() + round_robin_inter_dimension_scheduler,
                   dim_mapper.end());
            round_robin_inter_dimension_scheduler++;
            if (round_robin_inter_dimension_scheduler ==
                topology->get_num_of_dimensions()) {
                round_robin_inter_dimension_scheduler = 0;
            }
        } else if (collective_type != ComType::All_to_All &&
                   (inter_dimension_scheduling ==
                        InterDimensionScheduling::OfflineGreedy ||
                    inter_dimension_scheduling ==
                        InterDimensionScheduling::OfflineGreedyFlex)) {
            uint64_t prev_size = size;
            dim_mapper = offline_greedy->get_chunk_scheduling(
                num_streams, size, recommended_chunk_size, dimensions_involved,
                inter_dimension_scheduling, collective_type);
            chunk_size = prev_size - size;
        }

        if (collective_type == ComType::All_to_All ||
            (inter_dimension_scheduling !=
                 InterDimensionScheduling::OfflineGreedy &&
             inter_dimension_scheduling !=
                 InterDimensionScheduling::OfflineGreedyFlex)) {
            if (chunk_size > size) {
                size = 0;
            } else {
                size -= chunk_size;
            }
        }
        remain_size = chunk_size;
        std::list<CollectivePhase> vect;

        InjectionPolicy policy = InjectionPolicy::Normal;
        if (communicator_group && communicator_group->isLDSTImpl()) {
            policy = InjectionPolicy::LDSTBased;
        }

        if (collective_type != ComType::All_Reduce ||
            collectiveOptimization == CollectiveOptimization::Baseline) {
            for (int dim = 0; dim < topology->get_num_of_dimensions(); dim++) {
                if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) ==
                        1 ||
                    !dimensions_involved[dim_mapper[dim]]) {
                    continue;
                }
                std::pair<int, RingTopology::Direction> queue =
                    vLevels->get_next_queue_at_level(dim_mapper[dim]);

                CollectivePhase phase = generate_collective_phase(
                    collective_type,
                    topology->get_basic_topology_at_dimension(dim_mapper[dim],
                                                              collective_type),
                    remain_size, queue.first, queue.second, policy,
                    implementation_per_dimension[dim_mapper[dim]]);
                vect.push_back(phase);
                remain_size = phase.final_data_size;
            }
        } else if (inter_dimension_scheduling ==
                       InterDimensionScheduling::OfflineGreedy ||
                   inter_dimension_scheduling ==
                       InterDimensionScheduling::OfflineGreedyFlex ||
                   inter_dimension_scheduling ==
                       InterDimensionScheduling::OnlineGreedy) {
            int dim = 0;

            // Create collective phase for each dimension in ascending order.
            for (dim = 0; dim < topology->get_num_of_dimensions(); dim++) {
                if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) ==
                        1 ||
                    !dimensions_involved[dim_mapper[dim]]) {
                    continue;
                }
                std::pair<int, RingTopology::Direction> queue =
                    vLevels->get_next_queue_at_level_first(dim_mapper[dim]);
                CollectivePhase phase = generate_collective_phase(
                    ComType::Reduce_Scatter,
                    topology->get_basic_topology_at_dimension(
                        dim_mapper[dim], ComType::Reduce_Scatter),
                    remain_size, queue.first, queue.second, policy,
                    implementation_per_dimension[dim_mapper[dim]]);
                vect.push_back(phase);
                remain_size = phase.final_data_size;
            }
            dim--;

            // Create collective phases for each dimension in descending order.
            for (; dim >= 0; dim--) {
                if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) ==
                        1 ||
                    !dimensions_involved[dim_mapper[dim]]) {
                    continue;
                }
                std::pair<int, RingTopology::Direction> queue =
                    vLevels->get_next_queue_at_level_last(dim_mapper[dim]);
                CollectivePhase phase = generate_collective_phase(
                    ComType::All_Gather,
                    topology->get_basic_topology_at_dimension(
                        dim_mapper[dim], ComType::All_Gather),
                    remain_size, queue.first, queue.second, policy,
                    implementation_per_dimension[dim_mapper[dim]]);
                vect.push_back(phase);
                remain_size = phase.final_data_size;
            }
        } else {
            // In this branch, and the branch directly above, a collective
            // visits each dimension (excluding the last dimension) twice.
            // Specifically, for example, in 2D AllReduce, there would be 3
            // collective phases: Phase 0: Reduce Scatter in dim 0, Phase 1: All
            // Reduce in dim 1, Phase 2: All Gather in dim 0 Similarly, in 3D
            // AllReduce, there would be 5 collective phases: RS in dim 0, RS in
            // dim 1, AR in dim 2, AG in dim 1, AG in dim 0. Currently, queues
            // are allocated per dimension. If we allocate all queues in a
            // dimension to both phases of a single dimension, a race / deadlock
            // condition may occur. Therefore, in these cases, we have to
            // allocate half of the queues to the first phase, and the remaining
            // half to the second phase. (For example, in the above 2D case, if
            // we have 4 queues per dim, queues 0~1 are allocated to phase 0,
            // queues 2~3 are allocated to phase 2. For details, refer to
            // https://github.com/astra-sim/astra-sim/issues/137 and the linked
            // document.

            int dim = 0;
            int last_active_dim = 0;
            for (dim = 0; dim < topology->get_num_of_dimensions(); dim++) {
                if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) !=
                        1 &&
                    dimensions_involved[dim_mapper[dim]]) {
                    last_active_dim = dim;
                }
            }

            // Create collective phase for each dimension, excluding the last
            // dimension, in ascending order.
            for (dim = 0; dim < last_active_dim; dim++) {
                if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) ==
                        1 ||
                    !dimensions_involved[dim_mapper[dim]]) {
                    continue;
                }
                // Allocate the first half of queues available to this
                // dimension.
                std::pair<int, RingTopology::Direction> queue =
                    vLevels->get_next_queue_at_level_first(dim_mapper[dim]);
                CollectivePhase phase = generate_collective_phase(
                    ComType::Reduce_Scatter,
                    topology->get_basic_topology_at_dimension(
                        dim_mapper[dim], ComType::Reduce_Scatter),
                    remain_size, queue.first, queue.second, policy,
                    implementation_per_dimension[dim_mapper[dim]]);
                vect.push_back(phase);
                remain_size = phase.final_data_size;
            }
            while (dim > 0 && (dimensions_involved[dim_mapper[dim]] == false ||
                               topology->get_num_of_nodes_in_dimension(
                                   dim_mapper[dim]) == 1)) {
                dim--;
            }

            // The last dimension is the 'turning point'. Only one collective
            // phase is created.
            if (dimensions_involved[dim_mapper[dim]] &&
                topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) > 1) {
                // Despite only one collective phase being allocated to the last
                // dimension, we only allocate half of the queues available to
                // this dimension. This is because we want to match the number
                // of queues allocated to each collective phase. Processing
                // phases for this dim in n parallel queues, and queueing the
                // next phases in n/2 parallel queues could cause another
                // deadlock. Refer to the PR #135 for more details.
                std::pair<int, RingTopology::Direction> queue =
                    vLevels->get_next_queue_at_level_first(dim_mapper[dim]);
                CollectivePhase phase = generate_collective_phase(
                    ComType::All_Reduce,
                    topology->get_basic_topology_at_dimension(
                        dim_mapper[dim], ComType::All_Reduce),
                    remain_size, queue.first, queue.second, policy,
                    implementation_per_dimension[dim_mapper[dim]]);
                vect.push_back(phase);
                remain_size = phase.final_data_size;
            }
            dim--;

            // Create collective phases for each dimension, excluding the last
            // dimension, in descending order.
            for (; dim >= 0; dim--) {
                if (topology->get_num_of_nodes_in_dimension(dim_mapper[dim]) ==
                        1 ||
                    !dimensions_involved[dim_mapper[dim]]) {
                    continue;
                }
                // Allocate the second half of queues available to this
                // dimension.
                std::pair<int, RingTopology::Direction> queue =
                    vLevels->get_next_queue_at_level_last(dim_mapper[dim]);
                CollectivePhase phase = generate_collective_phase(
                    ComType::All_Gather,
                    topology->get_basic_topology_at_dimension(
                        dim_mapper[dim], ComType::All_Gather),
                    remain_size, queue.first, queue.second, policy,
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

            // TODO potentially create a LDST specific stream, if required
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

CollectivePhase SimpleSys::generate_collective_phase(
    ComType collective_type,
    BasicLogicalTopology* topology,
    uint64_t data_size,
    int queue_id,
    RingTopology::Direction direction,
    InjectionPolicy injection_policy,
    CollectiveImpl* collective_impl) {
    // for LDST based collectives
    // collective impl type supported now is direct
    if (injection_policy == InjectionPolicy::LDSTBased) {
        CollectivePhase vn(this, queue_id,
                           new AllToAll(collective_type,
                                        ((DirectCollectiveImpl*)collective_impl)
                                            ->direct_collective_window,
                                        id, (RingTopology*)topology, data_size,
                                        direction, injection_policy));
        return vn;
    }

    if (collective_impl->type == CollectiveImplType::Ring ||
        collective_impl->type == CollectiveImplType::OneRing) {
        CollectivePhase vn(this, queue_id,
                           new Ring(collective_type, id,
                                    (RingTopology*)topology, data_size,
                                    direction, injection_policy));
        return vn;
    } else if (collective_impl->type == CollectiveImplType::Direct ||
               collective_impl->type == CollectiveImplType::OneDirect) {
        CollectivePhase vn(
            this, queue_id,
            new AllToAll(
                collective_type,
                ((DirectCollectiveImpl*)collective_impl)
                    ->direct_collective_window,
                id, (RingTopology*)topology, data_size, direction,
                InjectionPolicy::
                    Normal /*TODO - should this be injection_policy*/));
        return vn;
    } else if (collective_impl->type == CollectiveImplType::DoubleBinaryTree) {
        CollectivePhase vn(this, queue_id,
                           new DoubleBinaryTreeAllReduce(
                               id, (BinaryTree*)topology, data_size));
        return vn;
    } else if (collective_impl->type == CollectiveImplType::HalvingDoubling ||
               collective_impl->type ==
                   CollectiveImplType::OneHalvingDoubling) {
        CollectivePhase vn(this, queue_id,
                           new HalvingDoubling(collective_type, id,
                                               (RingTopology*)topology,
                                               data_size));
        return vn;
    } else {
        std::cerr
            << "Error: No known collective implementation for collective phase"
            << std::endl;
        exit(1);
    }
}

uint64_t SimpleSys::determine_chunk_size(uint64_t& size, ComType type) {
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

int SimpleSys::get_priority(int explicit_priority) {
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

void SimpleSys::insert_into_ready_list(BaseStream* stream) {
    insert_stream(&ready_list, stream);
    scheduler_unit->notify_stream_added_into_ready_list();
}

void SimpleSys::insert_stream(std::list<BaseStream*>* queue,
                              BaseStream* baseStream) {
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
            } else if ((last == ComType::Reduce_Scatter &&
                        one_to_last == ComType::All_Gather) ||
                       (last == ComType::All_Gather &&
                        one_to_last == ComType::Reduce_Scatter)) {
                std::advance(it, 1);
                continue;
            } else {
                break;
            }
        }
    } else if (intra_dimension_scheduling ==
               IntraDimensionScheduling::SmallestFirst) {
        if (baseStream->phases_to_go.size() == 1) {
            it = queue->end();
        }
        while (it != queue->end()) {
            if ((*it)->initialized == true) {
                std::advance(it, 1);
                continue;
            } else if (std::max((*it)->my_current_phase.initial_data_size,
                                (*it)->my_current_phase.final_data_size) <
                       std::max(baseStream->my_current_phase.initial_data_size,
                                baseStream->my_current_phase.final_data_size)) {
                std::advance(it, 1);
                continue;
            } else {
                break;
            }
        }
    } else if (intra_dimension_scheduling ==
               IntraDimensionScheduling::LessRemainingPhaseFirst) {
        while (it != queue->end()) {
            if ((*it)->initialized == true) {
                std::advance(it, 1);
                continue;
            } else if ((*it)->phases_to_go.size() <
                       baseStream->phases_to_go.size()) {
                std::advance(it, 1);
                continue;
            } else {
                break;
            }
        }
    }
    queue->insert(it, baseStream);
}

void SimpleSys::schedule(int num) {
    int ready_list_size = ready_list.size();
    int counter = std::min(num, ready_list_size);
    while (counter > 0) {
        int top_vn = ready_list.front()->phases_to_go.front().queue_id;
        int total_waiting_streams = ready_list.size();
        int total_phases = ready_list.front()->phases_to_go.size();

        proceed_to_next_vnet_baseline((StreamBaseline*)ready_list.front());

        if (ready_list.front()->current_queue_id == -1) {
            Sys::sysPanic(
                "should not happen! " +
                std::to_string(
                    BaseStream::synchronizer[ready_list.front()->stream_id]) +
                " , " +
                std::to_string(
                    BaseStream::ready_counter[ready_list.front()->stream_id]) +
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

void SimpleSys::proceed_to_next_vnet_baseline(StreamBaseline* stream) {
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
        std::list<BaseStream*>& target =
            active_Streams.at(stream->my_current_phase.queue_id);
        for (std::list<BaseStream*>::iterator it = target.begin();
             it != target.end(); ++it) {
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
    if (stream->phases_to_go.front().queue_id < 0) {
        stream->phases_to_go.front().queue_id *= -1;
    }
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

DataSet* SimpleSys::issueCollective(WrapperNode& node,
                                    const std::vector<bool>& involved_dim,
                                    CommunicatorGroup* comm_group) {
    if (node.getCommType() == ChakraCollectiveCommType::ALL_REDUCE) {
        DataSet* fp = generate_all_reduce(node.getCommSize(), involved_dim,
                                          comm_group, node.getCommPriority());
        return fp;
    } else if (node.getCommType() == ChakraCollectiveCommType::ALL_TO_ALL) {
        DataSet* fp = generate_all_to_all(node.getCommSize(), involved_dim,
                                          comm_group, node.getCommPriority());
        return fp;
    } else if (node.getCommType() == ChakraCollectiveCommType::ALL_GATHER) {
        DataSet* fp = generate_all_gather(node.getCommSize(), involved_dim,
                                          comm_group, node.getCommPriority());
        return fp;
    } else if (node.getCommType() == ChakraCollectiveCommType::REDUCE_SCATTER) {
        DataSet* fp =
            generate_reduce_scatter(node.getCommSize(), involved_dim,
                                    comm_group, node.getCommPriority());
        return fp;
    } else if (node.getCommType() == ChakraCollectiveCommType::BROADCAST) {
        // broadcast colelctive has not been implemented in ASTRA-SIM yet.
        // So, we just use its real system mesurements
        uint64_t runtime = 1ul;
        if (node.getRuntime() != 0ul) {
            // getRuntime() is always in picoseconds
            runtime = node.getRuntime();
        }
        DataSet* fp = new DataSet(1);
        register_event(fp, EventType::General, nullptr, runtime);
        return fp;
    }
    return Sys::issueCollective(node, involved_dim, comm_group);
}

bool SimpleSys::claimEvent(EventType event, BasicEventHandlerData* ehd) {
    if (event == EventType::PacketReceived) {
        RecvPacketEventHandlerData* rcehd = (RecvPacketEventHandlerData*)ehd;
        if (rcehd->workload) {
            rcehd->workload->call(event, rcehd->wlhd);
        }
        if (rcehd->owner) {
            rcehd->owner->consume(rcehd);
        }
        delete rcehd;
        return true;
    }
    return false;
}

}  // namespace AstraSim
