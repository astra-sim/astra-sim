// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

// TODO: HardwareResource.cc should be moved to the system layer.

#include <astra-sim/workload/HardwareResource.hh>
#include <astra-sim/workload/Workload.hh>

using namespace AstraSim;
using namespace Chakra;

typedef ChakraProtoMsg::NodeType ChakraNodeType;

HardwareResource::HardwareResource(uint32_t num_npus)
    : num_npus(num_npus),
      num_in_flight_cpu_ops(0),
      num_in_flight_gpu_comm_ops(0),
      num_in_flight_gpu_comp_ops(0) {}

void HardwareResource::occupy(WrapperNode& node) {
    if (node.isCPUOp()) {
        assert(num_in_flight_cpu_ops == 0);
        ++num_in_flight_cpu_ops;
        return;
    }

    if (Workload::isCustomMscclCommNode(node)) {
        // For COMM_* nodes, we're assuming the system layer models hardware
        // resources, so always return true in the workload layer.
        return;
    }

    // check node type
    const auto node_type = node.getNodeType();
    if (node_type == ChakraNodeType::COMP_NODE) {
        assert(num_in_flight_gpu_comp_ops == 0);
        ++num_in_flight_gpu_comp_ops;
        return;
    }

    // collective comms
    assert(num_in_flight_gpu_comm_ops == 0);
    ++num_in_flight_gpu_comm_ops;
}

void HardwareResource::release(WrapperNode& node) {
    if (node.isCPUOp()) {
        --num_in_flight_cpu_ops;
        assert(num_in_flight_cpu_ops == 0);
        return;
    }

    if (Workload::isCustomMscclCommNode(node)) {
        // For COMM_* nodes, we're assuming the system layer models hardware
        // resources, so always return true in the workload layer.
        return;
    }

    // check node type
    const auto node_type = node.getNodeType();
    if (node_type == ChakraNodeType::COMP_NODE) {
        --num_in_flight_gpu_comp_ops;
        assert(num_in_flight_gpu_comp_ops == 0);
        return;
    }

    // collective comms
    --num_in_flight_gpu_comm_ops;
    assert(num_in_flight_gpu_comm_ops == 0);
}

bool HardwareResource::is_available(WrapperNode& node) const {
    if (node.isCPUOp()) {
        if (num_in_flight_cpu_ops == 0) {
            return true;
        } else {
            return false;
        }
    }

    if (Workload::isCustomMscclCommNode(node)) {
        // For COMM_* nodes, we're assuming the system layer models hardware
        // resources, so always return true in the workload layer.
        return true;
    }

    // check node type
    const auto node_type = node.getNodeType();
    if (node_type == ChakraNodeType::COMP_NODE) {
        if (num_in_flight_gpu_comp_ops == 0) {
            return true;
        } else {
            return false;
        }
    }

    // collective communication
    if (num_in_flight_gpu_comm_ops == 0) {
        return true;
    } else {
        return false;
    }
}
