/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/workload/HardwareResource.hh"

using namespace std;
using namespace AstraSim;
using namespace Chakra;

typedef ChakraProtoMsg::NodeType ChakraNodeType;

HardwareResource::HardwareResource(uint32_t num_npus)
    : num_npus(num_npus), num_in_flight_host_ops(0), num_in_flight_kernel_ops(0) {}

void HardwareResource::occupy(const shared_ptr<Chakra::ETFeederNode> node) {
  if (
      (node->getChakraNode()->type() == ChakraNodeType::COMP_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_SEND_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_RECV_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_COLL_NODE)) {
    ++num_in_flight_host_ops;
  } else if (
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMP_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_SEND_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_RECV_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_COLL_NODE)) {
    ++num_in_flight_kernel_ops;
  }
}

void HardwareResource::release(const shared_ptr<Chakra::ETFeederNode> node) {
  if (
      (node->getChakraNode()->type() == ChakraNodeType::COMP_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_SEND_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_RECV_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_COLL_NODE)) {
    --num_in_flight_host_ops;
  } else if (
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMP_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_SEND_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_RECV_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_COLL_NODE)) {
    --num_in_flight_kernel_ops;
  }
}

bool HardwareResource::is_available(
    const shared_ptr<Chakra::ETFeederNode> node) const {
  /*if ((node->getChakraNode()->type() == ChakraNodeType::COMP_NODE) &&
      (num_in_flight_host_ops >= num_npus)) {
    return false;
  } else {
    return true;
  }*/
  if (
      (node->getChakraNode()->type() == ChakraNodeType::COMP_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_SEND_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_RECV_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::COMM_COLL_NODE)) {
      
      if(num_in_flight_host_ops==0)
        return true;
      else
        return false;
  }
  else if (
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMP_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_SEND_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_RECV_NODE) ||
      (node->getChakraNode()->type() == ChakraNodeType::KERNEL_COMM_COLL_NODE)) {

      if(num_in_flight_kernel_ops==0)
        return true;
      else
        return false;
  }
  else{
      assert(false);
  }
}
