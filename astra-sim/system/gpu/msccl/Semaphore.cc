// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>

#include <astra-sim/system/gpu/msccl/Semaphore.hh>

#include <astra-sim/common/logging/Logger.hh>

using namespace AstraSim;
using namespace AstraSim::Msccl;

Semaphore::Semaphore(uint32_t _gpu_id,
                     uint32_t _connected_gpu_id,
                     uint32_t _sem_id,
                     uint32_t _init_val)
    : gpu_id(_gpu_id),
      connected_gpu_id(_connected_gpu_id),
      sem_id(_sem_id),
      init_val(_init_val) {
    if (init_val != 0) {
        THROW_ERR_NR("[Msccl::Semaphore] Semaphore initial value must be 0: "
                     "init_val=%d\n",
                     init_val);
    }
}

// Copy constructor
Semaphore::Semaphore(const Semaphore& other)
    : gpu_id(other.gpu_id),
      connected_gpu_id(other.connected_gpu_id),
      sem_id(other.sem_id),
      init_val(other.init_val) {}

// Assignment operator
Semaphore& Semaphore::operator=(const Semaphore& other) {
    if (this != &other) {
        gpu_id = other.gpu_id;
        connected_gpu_id = other.connected_gpu_id;
        sem_id = other.sem_id;
        init_val = other.init_val;
    }
    return *this;
}

// Compare operator
bool Semaphore::operator==(const Semaphore& other) const {
    return (gpu_id == other.gpu_id) &&
           (connected_gpu_id == other.connected_gpu_id) &&
           (sem_id == other.sem_id) && (init_val == other.init_val);
}

// To string
std::string Semaphore::to_string() const {
    std::stringstream ss;
    ss << "{gpu_id=" << gpu_id << ", connected_gpu_id=" << connected_gpu_id
       << ", sem_id=" << sem_id << ", init_val=" << init_val << "}";
    return ss.str();
}
