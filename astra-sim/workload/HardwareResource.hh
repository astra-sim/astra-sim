// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

// TODO: HardwareResource.hh should be moved to the system layer.

#pragma once

#include <cstdint>
#include <feeder/wrapper_node.h>

namespace AstraSim {

class HardwareResource {
  public:
    HardwareResource(uint32_t num_npus);
    void occupy(WrapperNode& node);
    void release(WrapperNode& node);
    bool is_available(WrapperNode& node) const;

    const uint32_t num_npus;
    uint32_t num_in_flight_cpu_ops;
    uint32_t num_in_flight_gpu_comp_ops;
    uint32_t num_in_flight_gpu_comm_ops;
};

}  // namespace AstraSim
