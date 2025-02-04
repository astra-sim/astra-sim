/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

// TODO: HardwareResource.hh should be moved to the system layer.

#ifndef __HARDWARE_RESOURCE_HH__
#define __HARDWARE_RESOURCE_HH__

#include <cstdint>

#include "extern/graph_frontend/chakra/src/feeder/et_feeder.h"

namespace AstraSim {

class HardwareResource {
  public:
    HardwareResource(uint32_t num_npus);
    void occupy(const std::shared_ptr<Chakra::ETFeederNode> node);
    void release(const std::shared_ptr<Chakra::ETFeederNode> node);
    bool is_available(const std::shared_ptr<Chakra::ETFeederNode> node) const;
    void report();

    std::shared_ptr<Chakra::ETFeederNode> cpu_ops_node;
    std::shared_ptr<Chakra::ETFeederNode> gpu_ops_node;
    std::shared_ptr<Chakra::ETFeederNode> gpu_comms_node;

    const uint32_t num_npus;
    uint32_t num_in_flight_cpu_ops;
    uint32_t num_in_flight_gpu_comp_ops;
    uint32_t num_in_flight_gpu_comm_ops;

    uint64_t num_cpu_ops;
    uint64_t num_gpu_ops;
    uint64_t num_gpu_comms;

    uint64_t tics_cpu_ops;
    uint64_t tics_gpu_ops;
    uint64_t tics_gpu_comms;
};

}  // namespace AstraSim

#endif /* __HARDWARE_RESOURCE_HH__ */
