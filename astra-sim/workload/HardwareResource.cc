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
    : num_npus(num_npus), num_in_flight_cpu_ops(0), num_in_flight_gpu_comm_ops(0), 
    num_in_flight_gpu_comp_ops(0) {}

void HardwareResource::occupy(const shared_ptr<Chakra::ETFeederNode> node) {
  if (node->is_cpu_op()) {
    assert(num_in_flight_cpu_ops == 0);
    ++num_in_flight_cpu_ops;
  } else {
    if (node->type() == ChakraNodeType::COMP_NODE){
      assert(num_in_flight_gpu_comp_ops == 0);
      ++num_in_flight_gpu_comp_ops;
    } else{
      if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
        return; 
      }
      assert(num_in_flight_gpu_comm_ops == 0);
      ++num_in_flight_gpu_comm_ops;
      }
  }
}

void HardwareResource::release(const shared_ptr<Chakra::ETFeederNode> node) {
  if (node->is_cpu_op()) {
    --num_in_flight_cpu_ops;
    assert(num_in_flight_cpu_ops == 0);
  } else {
    if (node->type() == ChakraNodeType::COMP_NODE){
      --num_in_flight_gpu_comp_ops;
      assert(num_in_flight_gpu_comp_ops == 0);
    } else {
      if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
        return;
      }
      --num_in_flight_gpu_comm_ops;
      assert(num_in_flight_gpu_comm_ops == 0);
    }
  }
}

bool HardwareResource::is_available(
    const shared_ptr<Chakra::ETFeederNode> node) const {
  if (node->is_cpu_op()) {
      if (num_in_flight_cpu_ops == 0) {
        return true;
      } else {
        return false;
      }
  } else {
      if (node->type() == ChakraNodeType::COMP_NODE){
        if (num_in_flight_gpu_comp_ops == 0) {
          return true;
        } else {
          return false;
        }
      } else {
        if (node->type() == ChakraNodeType::COMM_RECV_NODE){
          return true;
        }
        if (num_in_flight_gpu_comm_ops == 0) {
          return true;
        } else {
          return false;
        }
      }
  }
}
