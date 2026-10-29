// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>

namespace AstraSim {

class SimpleSys;
class Algorithm;
class BaseStream;

class CollectivePhase {
  public:
    CollectivePhase(SimpleSys* sys, int queue_id, Algorithm* algorithm);
    CollectivePhase();
    void init(BaseStream* stream);

    SimpleSys* sys;
    int queue_id;
    Algorithm* algorithm;
    uint64_t initial_data_size;
    uint64_t final_data_size;
    bool enabled;
    ComType comm_type;
};

}  // namespace AstraSim
