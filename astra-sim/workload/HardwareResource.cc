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
    : num_npus(num_npus),
      num_in_flight_cpu_comp_ops(0u),
      num_in_flight_gpu_comp_ops(0u),
      num_in_flight_comm_ops(0u),
      num_in_flight_mem_ops(0u) {}

void HardwareResource::occupy(const shared_ptr<Chakra::ETFeederNode> node) {
  switch (node->type()) {
    case ChakraNodeType::COMP_NODE:
      if (node->is_cpu_op(true)) {
        assert(num_in_flight_cpu_comp_ops == 0);
        ++num_in_flight_cpu_comp_ops;
      } else {
        assert(num_in_flight_gpu_comp_ops == 0);
        ++num_in_flight_gpu_comp_ops;
      }
      break;
    case ChakraNodeType::COMM_COLL_NODE:
    case ChakraNodeType::COMM_SEND_NODE:
    case ChakraNodeType::COMM_RECV_NODE:
      assert(num_in_flight_comm_ops == 0);
      ++num_in_flight_comm_ops;
      break;
    case ChakraNodeType::MEM_LOAD_NODE:
    case ChakraNodeType::MEM_STORE_NODE:
      assert(num_in_flight_mem_ops == 0);
      ++num_in_flight_mem_ops;
      break;
    case ChakraNodeType::INVALID_NODE:
      break;
    default:
      assert(false);
  }
}

void HardwareResource::release(const shared_ptr<Chakra::ETFeederNode> node) {
  switch (node->type()) {
    case ChakraNodeType::COMP_NODE:
      if (node->is_cpu_op(true)) {
        assert(num_in_flight_cpu_comp_ops > 0);
        --num_in_flight_cpu_comp_ops;
      } else {
        assert(num_in_flight_gpu_comp_ops > 0);
        --num_in_flight_gpu_comp_ops;
      }
      break;
    case ChakraNodeType::COMM_COLL_NODE:
    case ChakraNodeType::COMM_SEND_NODE:
    case ChakraNodeType::COMM_RECV_NODE:
      assert(num_in_flight_comm_ops > 0);
      --num_in_flight_comm_ops;
      break;
    case ChakraNodeType::MEM_LOAD_NODE:
    case ChakraNodeType::MEM_STORE_NODE:
      assert(num_in_flight_mem_ops > 0);
      --num_in_flight_mem_ops;
      break;
    case ChakraNodeType::INVALID_NODE:
      break;
    default:
      assert(false);
  }
}

bool HardwareResource::is_available(
    const shared_ptr<Chakra::ETFeederNode> node) const {
  switch (node->type()) {
    case ChakraNodeType::COMP_NODE:
      if (node->is_cpu_op(true))
        return num_in_flight_cpu_comp_ops == 0;
      else
        return num_in_flight_gpu_comp_ops == 0;

    case ChakraNodeType::COMM_COLL_NODE:
    case ChakraNodeType::COMM_SEND_NODE:
    case ChakraNodeType::COMM_RECV_NODE:
      return num_in_flight_comm_ops == 0;

    case ChakraNodeType::MEM_LOAD_NODE:
    case ChakraNodeType::MEM_STORE_NODE:
      return num_in_flight_mem_ops == 0;

    case ChakraNodeType::INVALID_NODE:
      return true;

    default:
      assert(false);
  }
}
