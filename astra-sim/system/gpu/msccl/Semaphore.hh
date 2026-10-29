// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cstdint>
#include <string>

namespace AstraSim {
namespace Msccl {

struct Semaphore {
    uint32_t gpu_id;            // ID of GPU where the semaphore resides
    uint32_t connected_gpu_id;  // ID of connected GPU
    uint32_t sem_id;            // Semaphore ID
    uint32_t init_val;          // Initial value

    // Constructor & destructor
    Semaphore(uint32_t _gpu_id,
              uint32_t _connected_gpu_id,
              uint32_t _sem_id,
              uint32_t _init_val);
    ~Semaphore() = default;

    // Copy constructor
    Semaphore(const Semaphore& other);

    // Assignment operator
    Semaphore& operator=(const Semaphore& other);

    // Compare operator
    bool operator==(const Semaphore& other) const;

    // To string
    std::string to_string() const;
};

}  // namespace Msccl
}  // namespace AstraSim
