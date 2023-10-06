/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/workload/HardwareResource.hh"
#include <assert.h> 

using namespace std;
using namespace AstraSim;
using namespace Chakra;

typedef ChakraProtoMsg::NodeType ChakraNodeType;

HardwareResource::HardwareResource(uint32_t num_npus)
    : num_npus(num_npus), num_in_flight_host_ops(0), num_in_flight_kernel_ops(0) {}

void HardwareResource::occupy(const shared_ptr<Chakra::ETFeederNode> node) {
  if (node->is_host_op()) {
    assert(num_in_flight_host_ops==0);
    ++num_in_flight_host_ops;
  } else {
    assert(num_in_flight_kernel_ops==0);
    ++num_in_flight_kernel_ops;
  }
}

void HardwareResource::release(const shared_ptr<Chakra::ETFeederNode> node) {
  if (node->is_host_op()) {
    --num_in_flight_host_ops;
    assert(num_in_flight_host_ops==0);
  } else {
    --num_in_flight_kernel_ops;
    assert(num_in_flight_kernel_ops==0);
  }
}

bool HardwareResource::is_available(
    const shared_ptr<Chakra::ETFeederNode> node) const {
  if (node->is_host_op()) {
      if (num_in_flight_host_ops == 0) {
        return true;
      } else {
        return false;
      }
  } else {
      if (num_in_flight_kernel_ops == 0) {
        return true;
      } else {
        return false;
      }
  }
}
