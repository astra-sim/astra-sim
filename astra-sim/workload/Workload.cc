// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <fstream>
#include <iostream>
#include <json/json.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/common/stats/StatCollection.hh>
#include <astra-sim/system/common/MemEventHandlerData.hh>
#include <astra-sim/system/common/Sys.hh>
#include <astra-sim/system/common/WorkloadLayerHandlerData.hh>
#include <astra-sim/system/simple/CommunicatorGroup.hh>
#include <astra-sim/system/simple/IntData.hh>
#include <astra-sim/system/simple/RecvPacketEventHandlerData.hh>
#include <astra-sim/system/simple/SimpleSys.hh>
#include <astra-sim/workload/Workload.hh>

using namespace AstraSim;
using namespace Chakra;
using json = nlohmann::json;

namespace {

/** Returns whether a workload JSON is an MSCCL++ plan, not a Chakra graph. */
bool isMscclPlan(const std::string& filename) {
    auto in_file = std::ifstream{filename};
    const auto data = json::parse(in_file);
    return !data.contains("workload_graph");
}

}  // namespace

typedef JSONNode::NodeType ChakraNodeType;
typedef JSONNode::CollectiveCommType ChakraCollectiveCommType;

uint32_t Workload::s_num_outstanding_workloads = 0;

Workload::Workload(Sys* sys,
                   std::string workload_filename,
                   std::string comm_group_filename,
                   uint64_t comm_size,
                   std::string custom_comm_plan_map_filename) {
    std::string ext =
        workload_filename.substr(workload_filename.find_last_of(".") + 1);
    std::string raw_filename =
        workload_filename.substr(0, workload_filename.find_last_of("."));
    std::string full_workload_filename;
    if (ext == "et") {
        full_workload_filename =
            raw_filename + "." + std::to_string(sys->id) + ".et";
    } else if (ext == "json") {
        // One JSON file describes every rank: a Chakra graph or an MSCCL++ plan
        full_workload_filename = workload_filename;
    } else {
        std::cerr << "Unknown input workload file format" << std::endl
                  << "WORKLOAD can be either protobuf or json" << std::endl
                  << "Omit the file number and provide the name and extension "
                     "of the workload file"
                  << std::endl
                  << "Examples - " << std::endl
                  << "Protobuf: For simulating a 2 NPU system, you'll have "
                     "xyz.0.et and xyz.1.et"
                  << std::endl
                  << "Input WORKLOAD=\"xyz.et\"" << std::endl
                  << "JSON: For simulating a 2 NPU system, you'll have "
                     "xyz.0.json and xyz.1.json"
                  << std::endl
                  << "Input WORKLOAD=\"xyz.json\"" << std::endl;
    }

    // Check if workload filename exists
    if (access(full_workload_filename.c_str(), R_OK) < 0) {
        std::string error_msg;
        if (errno == ENOENT) {
            error_msg =
                "workload file: " + full_workload_filename + " does not exist";
        } else if (errno == EACCES) {
            error_msg = "workload file: " + full_workload_filename +
                        " exists but is not readable";
        } else {
            error_msg = "Unknown workload file: " + full_workload_filename +
                        " access error";
        }
        std::cerr << error_msg << std::endl;
        exit(EXIT_FAILURE);
    }

    if (ext == "et") {
        // Chakra ET: one Protobuf file per rank
        this->node.createWrapper(full_workload_filename);
    } else if (ext == "json") {
        if (isMscclPlan(full_workload_filename)) {
            // MSCCL++ plan: one custom-collective node shared by every rank
            this->node.createSingleCustomCommNode(full_workload_filename,
                                                  comm_size);
        } else {
            // Chakra JSON workload graph describing every rank
            this->node.createWrapper(full_workload_filename);
        }
    } else {
        Sys::sysPanic("Unsupported workload file extension: " + ext);
    }
    loadCustomCommPlanMap(custom_comm_plan_map_filename);

    this->comm_group = nullptr;
    // TODO: parametrize the number of available hardware resources
    this->hw_resource = new HardwareResource(1);
    this->sys = sys;
    // Should we just need to deprecate comm_group?
    // initialize_comm_group(comm_group_filename);
    this->is_finished = false;

    // Mark this workload as outstanding
    s_num_outstanding_workloads++;
}

Workload::~Workload() {
    node.releaseMemory();
    if (this->comm_group != nullptr) {
        delete this->comm_group;
    }
    if (this->hw_resource != nullptr) {
        delete this->hw_resource;
    }
}

void Workload::loadCustomCommPlanMap(const std::string& plan_map_filename) {
    if (plan_map_filename.empty()) {
        return;
    }

    if (access(plan_map_filename.c_str(), R_OK) < 0) {
        std::cerr << "custom comm plan map file: " << plan_map_filename
                  << " does not exist or is not readable" << std::endl;
        exit(EXIT_FAILURE);
    }

    const auto plan_map = YAML::LoadFile(plan_map_filename);
    const auto entries = plan_map["custom_comm_plan_file"];
    if (!entries || !entries.IsMap()) {
        std::cerr << "custom comm plan map file: " << plan_map_filename
                  << " is missing a top-level custom_comm_plan_file map"
                  << std::endl;
        exit(EXIT_FAILURE);
    }

    // Same base-directory idiom as WrapperNode::resolvePlanPaths(): a
    // relative plan path resolves against this yml file's own directory.
    const auto slash = plan_map_filename.rfind('/');
    const std::string base_dir = (slash == std::string::npos)
                                     ? ""
                                     : plan_map_filename.substr(0, slash + 1);

    std::unordered_map<uint64_t, std::string> plan_map_by_id;
    for (const auto& entry : entries) {
        const auto node_id = entry.first.as<uint64_t>();
        auto plan_file = entry.second.as<std::string>();
        if (!plan_file.empty() && plan_file.front() != '/') {
            plan_file = base_dir + plan_file;
        }
        plan_map_by_id[node_id] = plan_file;
    }
    node.setCustomCommPlanMap(std::move(plan_map_by_id));
}

void Workload::initialize_comm_group(std::string comm_group_filename) {
    // communicator group input file is not given
    if (comm_group_filename.find("empty") != std::string::npos) {
        comm_group = nullptr;
        return;
    }

    std::ifstream inFile;
    json j;
    inFile.open(comm_group_filename);
    inFile >> j;

    std::vector<int> involved_NPUs;
    for (json::iterator it = j.begin(); it != j.end(); ++it) {
        bool in_comm_group = false;
        bool isLDST = false;

        involved_NPUs.clear();
        // entries per comm_group
        // check if json uses newer LD/ST format or old GT format
        if (it.value().contains("implGranularity") &&
            it.value().contains("involvedNPUs")) {
            for (json::iterator it2 = it.value().begin();
                 it2 != it.value().end(); ++it2) {
                if (it2.key() == "implGranularity") {
                    if (it2.value() == "LDST") {
                        isLDST = true;
                    } else if (it2.value() == "LDST") {
                        isLDST = false;
                    } else {
                        assert(false && "Unknown implGranularity! valid "
                                        "options: LDST or SDMA");
                    }

                } else if (it2.key() == "involvedNPUs") {
                    for (auto id : it2.value()) {
                        involved_NPUs.push_back(id);
                        if (id == sys->id) {
                            in_comm_group = true;
                        }
                    }
                }
            }
        } else {
            for (auto id : it.value()) {
                involved_NPUs.push_back(id);
                if (id == sys->id) {
                    in_comm_group = true;
                }
            }
        }

        if (in_comm_group) {
            DPRINTFNR("Loading comm_group: involvedNPUs size: %lu, "
                      "isLDST: %u\n",
                      involved_NPUs.size(), isLDST);

            comm_group = new CommunicatorGroup(1, involved_NPUs,
                                               static_cast<SimpleSys*>(sys));
            comm_group->setLDSTImpl(isLDST);
            // Note: All NPUs should create comm group with identical ids if
            // they want to communicate with each other
        }
    }
}

void Workload::issue_dep_free_nodes() {
    node.getNextIssuableNode();
    while (node.isValidNode()) {
        DPRINTFNR("Issuing dependency-free node id=%lu\n", node.getNodeID());
        if (hw_resource->is_available(node)) {
            issue(node);
        } else {
            node.push_to_queue();
        }
        node.getNextIssuableNode();
    }

    // TODO: replace these functions
    while (!node.is_queue_empty()) {
        node.queue_front();
        node.pushBackIssuableNode(node.getNodeID());
        node.pop_from_queue();
    }
}

void Workload::issue(WrapperNode& node) {
    if (sys->replay_only) {
        hw_resource->occupy(node);
        issue_replay(node);
        return;
    }

    const auto node_type = node.getNodeType();
    if ((node_type == ChakraNodeType::METADATA_NODE) ||
        (node_type == ChakraNodeType::INVALID_NODE)) {
        // Not a schedulable operation: drop it and unblock its children.
        skip_invalid(node);
    } else if ((node_type == ChakraNodeType::MEM_LOAD_NODE) ||
               (node_type == ChakraNodeType::MEM_STORE_NODE)) {
        issue_remote_mem(node);
    } else if (node.isCPUOp() || (node_type == ChakraNodeType::COMP_NODE)) {
        if ((node.getRuntime() == 0) && (node.getNumOps() == 0)) {
            skip_invalid(node);
        } else {
            issue_comp(node);
        }
    } else if (node_type == ChakraNodeType::COMM_COLL_NODE) {
        // issue_comm() dispatches on the node's specific comm_type, including
        // CUSTOM_MSCCL.
        issue_comm(node);
    } else {
        std::cerr << "Invalid node: " << node.getNodeType() << std::endl;
        exit(EXIT_FAILURE);
    }
}

void Workload::issue_replay(WrapperNode& node) {
    WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
    wlhd->node_id = node.getNodeID();
    uint64_t runtime = 1ul;
    if (node.getRuntime() != 0ul) {
        // getRuntime() is always in picoseconds
        runtime = node.getRuntime();
    }
    sys->register_event(this, EventType::General, wlhd, runtime);
}

void Workload::issue_remote_mem(WrapperNode& node) {
    hw_resource->occupy(node);

    WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
    wlhd->sys_id = sys->id;
    wlhd->workload = this;

    wlhd->node_id = node.getNodeID();

    // There are cases when AstraRemoteMemoryAPI are not required
    // (e.g, remote memory is not modelled / not relevant), not
    // implementing AstraRemoteMemoryAPI can reduce simulation events.
    // if AstraRemoteMemoryAPI (sys->remote_mem) is null / not implemented,
    // simply call the callback function
    if (!sys->remote_mem) {
        wlhd->workload->call(EventType::General, wlhd);
        return;
    }

    sys->remote_mem->issue(node.getTensorSize(), wlhd);
}

void Workload::issue_comp(WrapperNode& node) {
    // A zero tensor size is an invalid node in roofline mode; skip it before
    // occupying a hardware slot, rather than divide by zero when computing
    // operational intensity below.
    if (sys->roofline_enabled && node.getTensorSize() == 0) {
        skip_invalid(node);
        return;
    }

    hw_resource->occupy(node);
    if (sys->roofline_enabled) {
        WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
        wlhd->node_id = node.getNodeID();

        double operational_intensity =
            static_cast<double>(node.getNumOps()) /
            static_cast<double>(node.getTensorSize());
        double perf = sys->roofline->get_perf(operational_intensity);
        // elapsed_time is in seconds (ops / (ops/s)); register_event expects
        // picoseconds, so scale by 1e12 (was missing, making roofline compute
        // time silently 0 for any op under 1 second).
        double elapsed_time = static_cast<double>(node.getNumOps()) / perf;
        uint64_t runtime = static_cast<uint64_t>(elapsed_time * 1e12);
        sys->register_event(this, EventType::General, wlhd, runtime);
    } else {
        // advance this node forward the recorded "replayed" time specificed in
        // the ET.
        issue_replay(node);
    }
}

void Workload::issue_comm(WrapperNode& node) {
    hw_resource->occupy(node);

    if (node.isCPUOp() ||
        (node.getNodeType() != ChakraNodeType::COMM_COLL_NODE)) {
        std::cerr << "Unknown communication node type" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::vector<bool> involved_dim;

    // involved_dim does not exist in ETFeeder anymore.
    // We assume involved_dim equals 1, until Chakra supports communicator
    // group.
    involved_dim.push_back(true);

    DataSet* fp = sys->issueCollective(node, involved_dim, comm_group);
    if (fp != nullptr) {
        collective_comm_node_id_map[fp->my_id] = node.getNodeID();
        collective_comm_wrapper_map[fp->my_id] = fp;
        fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
    }
}

void Workload::skip_invalid(WrapperNode& node) {
    node.freeChildrenNodes(node.getNodeID());
    node.removeNode(node.getNodeID());
}

void Workload::_complete_node(WorkloadLayerHandlerData* wlhd) {
    // Release the finished node's hardware slot, unblock its children, issue
    // whatever just became dependency-free, then drop it from the graph.
    node.lookupNode(wlhd->node_id);
    hw_resource->release(node);
    node.freeChildrenNodes(node.getNodeID());
    issue_dep_free_nodes();
    node.removeNode(wlhd->node_id);
    delete wlhd;
}

void Workload::call(EventType event, CallData* data) {
    if (is_finished) {
        return;
    }

    if (event == EventType::CollectiveCommunicationFinished) {
        IntData* int_data = (IntData*)data;
        uint64_t coll_comm_id = int_data->data;
        uint64_t node_id = collective_comm_node_id_map[coll_comm_id];
        node.lookupNode(node_id);
        hw_resource->release(node);
        node.freeChildrenNodes(node_id);

        issue_dep_free_nodes();

        node.removeNode(node_id);

        // The Dataset class provides statistics that should be used later to
        // dump more statistics in the workload layer
        delete collective_comm_wrapper_map[coll_comm_id];
        collective_comm_wrapper_map.erase(coll_comm_id);

    } else if (event == EventType::Workload_NodeFinished) {
        auto wlhd = dynamic_cast<WorkloadLayerHandlerData*>(data);
        assert(wlhd);
        _complete_node(wlhd);
    } else if (event == EventType::General) {
        if (data == nullptr) {
            // Plain wakeup with no specific node attached (e.g.
            // Workload::fire() kicking off the simulation): just re-scan for
            // issuable nodes.
            issue_dep_free_nodes();
        } else {
            // A specific node finished (e.g. issue_replay()'s completion
            // callback): release its resources like Workload_NodeFinished
            // does.
            _complete_node((WorkloadLayerHandlerData*)data);
        }
    } else {
        THROW_ERR_NR("Workload::call() reached an unhandled event type: %d\n",
                     static_cast<int>(event));
    }

    if (!node.hasNodesToIssue() && (hw_resource->num_in_flight_cpu_ops == 0) &&
        (hw_resource->num_in_flight_gpu_comp_ops == 0) &&
        (hw_resource->num_in_flight_gpu_comm_ops == 0)) {
        report();
        is_finished = true;
    }
}

void Workload::fire() {
    call(EventType::General, nullptr);
}

void Workload::report() {
    Tick curr_tick = Sys::boostedTick();
    std::cout << "sys[" << sys->id << "] finished, " << curr_tick << " ticks"
              << std::endl;

    // Update the total number of ticks
    sys->updateFinalNumTicks(curr_tick);
    assert(s_num_outstanding_workloads > 0);
    s_num_outstanding_workloads--;

    // Dump stats only when all workloads completed
    if (s_num_outstanding_workloads == 0) {
        g_stat_collection.dumpStats(true);
    }
}

bool Workload::isCustomMscclCommNode(WrapperNode& node) {
    return (node.getNodeType() == ChakraNodeType::COMM_COLL_NODE) &&
           (node.getCommType() == ChakraCollectiveCommType::CUSTOM_MSCCL);
}
