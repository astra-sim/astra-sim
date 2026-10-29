// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <string>
#include <unordered_map>

#include <astra-sim/system/common/Callable.hh>
#include <astra-sim/workload/HardwareResource.hh>
#include <feeder/wrapper_node.h>

namespace AstraSim {

class Sys;
class CommunicatorGroup;
class DataSet;
class WorkloadLayerHandlerData;
enum class FileType { XML, JSON, PROTOBUF };
class Workload : public Callable {
  public:
    Workload(Sys* sys,
             std::string workload_filename,
             std::string comm_group_filename,
             uint64_t comm_size = 0,
             std::string custom_comm_plan_map_filename = "");
    ~Workload();

    // communicator groups
    void initialize_comm_group(std::string comm_group_filename);

    // event-based simulation
    void issue_dep_free_nodes();
    void issue(WrapperNode& node);
    void issue_replay(WrapperNode& node);
    void issue_remote_mem(WrapperNode& node);
    void issue_comp(WrapperNode& node);
    void issue_comm(WrapperNode& node);
    void skip_invalid(WrapperNode& node);
    void call(EventType event, CallData* data);
    void fire();

    // stats
    void report();

    // Return true if the given node is a custom MSCCL node
    static bool isCustomMscclCommNode(WrapperNode& node);

    // Chakra::ETFeeder* et_feeder;
    CommunicatorGroup* comm_group;
    HardwareResource* hw_resource;
    Sys* sys;
    std::unordered_map<int, uint64_t> collective_comm_node_id_map;
    std::unordered_map<int, DataSet*> collective_comm_wrapper_map;
    bool is_finished;
    WrapperNode node;

  private:
    /**
     * Read a YAML file mapping Chakra node IDs to custom collective plan
     * files and overwrite each named node's plan path.
     *
     * @param plan_map_filename path to the YAML map, or empty for a no-op
     */
    void loadCustomCommPlanMap(const std::string& plan_map_filename);

    // Release a finished node's resources, unblock its children, and issue
    // any newly dependency-free nodes.
    void _complete_node(WorkloadLayerHandlerData* wlhd);

    // Number of outstanding workload instances
    static uint32_t s_num_outstanding_workloads;
};

}  // namespace AstraSim
