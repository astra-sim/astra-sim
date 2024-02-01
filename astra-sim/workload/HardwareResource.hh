/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

// TODO: HardwareResource.hh should be moved to the system layer.

#ifndef __HARDWARE_RESOURCE_HH__
#define __HARDWARE_RESOURCE_HH__

#include <cstdint>

#include "extern/graph_frontend/chakra/et_feeder/et_feeder.h"

namespace AstraSim {

class HardwareResource {
 public:
  HardwareResource(uint32_t num_npus);
  void occupy(const std::shared_ptr<Chakra::ETFeederNode> node);
  void release(const std::shared_ptr<Chakra::ETFeederNode> node);
  bool is_available(const std::shared_ptr<Chakra::ETFeederNode> node) const;

  const uint32_t num_npus;
  uint32_t num_in_flight_cpu_comp_ops;
  uint32_t num_in_flight_gpu_comp_ops;
  uint32_t num_in_flight_comm_ops;
  uint32_t num_in_flight_mem_ops;
};

} // namespace AstraSim

#endif /* __HARDWARE_RESOURCE_HH__ */
