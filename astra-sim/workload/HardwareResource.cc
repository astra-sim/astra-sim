/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

// TODO: HardwareResource.cc should be moved to the system layer.

#include "astra-sim/workload/HardwareResource.hh"

using namespace std;
using namespace AstraSim;
using namespace Chakra;

typedef ChakraProtoMsg::NodeType ChakraNodeType;

HardwareResource::HardwareResource(uint32_t num_npus)
    : num_npus(num_npus), num_in_flight_cpu_comp_ops(0u), num_in_flight_gpu_comp_ops(0u), 
    num_in_flight_comm_send_ops(0u), num_in_flight_comm_recv_ops(0u), num_in_flight_mem_load_ops(0u),
    num_in_flight_mem_store_ops(0u) {}

void HardwareResource::occupy(const shared_ptr<Chakra::ETFeederNode> node) {
  if (
    node->type() == ChakraNodeType::COMM_COLL_NODE ||
    node->type() == ChakraNodeType::COMM_SEND_NODE
  ) {
    assert(num_in_flight_comm_send_ops == 0);
    this->num_in_flight_comm_send_ops++;
  }
  else if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
    this->num_in_flight_comm_recv_ops++;
  }
  else if (node->type() == ChakraNodeType::COMP_NODE) {
    if (node->is_cpu_op()) {
      assert(num_in_flight_cpu_comp_ops == 0);
      this->num_in_flight_cpu_comp_ops++;
    }
    else {
      assert(num_in_flight_gpu_comp_ops == 0);
      this->num_in_flight_gpu_comp_ops++;
    }
  }
  else if (node->type() == ChakraNodeType::MEM_LOAD_NODE) {
    assert(num_in_flight_mem_load_ops == 0);
    this->num_in_flight_mem_load_ops++;
  }
  else if (node->type() == ChakraNodeType::MEM_STORE_NODE) {
    assert(num_in_flight_mem_store_ops == 0);
    this->num_in_flight_mem_store_ops++;
  }
  else if (node->type() == ChakraNodeType::INVALID_NODE) {
    // pass
  }
  else {
    cerr << "invalid node type" << endl;
    exit(-1);
  }
}

void HardwareResource::release(const shared_ptr<Chakra::ETFeederNode> node) {
  if (
    node->type() == ChakraNodeType::COMM_COLL_NODE ||
    node->type() == ChakraNodeType::COMM_SEND_NODE
  ) {
    this->num_in_flight_comm_send_ops--;
  }
  else if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
    this->num_in_flight_comm_recv_ops--;
  }
  else if (node->type() == ChakraNodeType::COMP_NODE) {
    if (node->is_cpu_op()) {
      this->num_in_flight_cpu_comp_ops--;
    }
    else {
      this->num_in_flight_gpu_comp_ops--;
    }
  }
  else if (node->type() == ChakraNodeType::MEM_LOAD_NODE) {
    this->num_in_flight_mem_load_ops--;
  }
  else if (node->type() == ChakraNodeType::MEM_STORE_NODE) {
    this->num_in_flight_mem_store_ops--;
  }
  else if (node->type() == ChakraNodeType::INVALID_NODE) {
    // pass
  }
  else {
    cerr << "invalid node type" << endl;
    exit(-1);
  }
}

bool HardwareResource::is_available(
    const shared_ptr<Chakra::ETFeederNode> node) const {
  if (
    node->type() == ChakraNodeType::COMM_COLL_NODE ||
    node->type() == ChakraNodeType::COMM_SEND_NODE
  ) {
    return this->num_in_flight_comm_send_ops < 1u;
  }
  else if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
    return true;
  }
  else if (node->type() == ChakraNodeType::COMP_NODE) {
    if (node->is_cpu_op()) {
      return this->num_in_flight_cpu_comp_ops < 1u;
    }
    else {
      return this->num_in_flight_gpu_comp_ops < 1u;
    }
  }
  else if (node->type() == ChakraNodeType::MEM_LOAD_NODE) {
    return this->num_in_flight_mem_load_ops < 1u;
  }
  else if (node->type() == ChakraNodeType::MEM_STORE_NODE) {
    return this->num_in_flight_mem_store_ops < 1u;
  }
  else if (node->type() == ChakraNodeType::INVALID_NODE) {
    return true;
  }
  else {
    cerr << "invalid node type" << endl;
    exit(-1);
  }
}

bool HardwareResource::all_resources_released() const {
  if (num_in_flight_cpu_comp_ops != 0)
    return false;
  if (num_in_flight_gpu_comp_ops != 0)
    return false;
  if (num_in_flight_comm_send_ops != 0)
    return false;
  if (num_in_flight_comm_recv_ops != 0)
    return false;
  if (num_in_flight_mem_load_ops != 0)
    return false;
  if (num_in_flight_mem_store_ops != 0)
    return false;
  return true;
}
