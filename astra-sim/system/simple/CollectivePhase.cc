// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/CollectivePhase.hh>

#include <astra-sim/system/simple/astraccl/Algorithm.hh>

using namespace AstraSim;

CollectivePhase::CollectivePhase(SimpleSys* sys,
                                 int queue_id,
                                 Algorithm* algorithm) {
    this->sys = sys;
    this->queue_id = queue_id;
    this->algorithm = algorithm;
    this->enabled = true;
    this->initial_data_size = algorithm->data_size;
    this->final_data_size = algorithm->final_data_size;
    this->comm_type = algorithm->comType;
    this->enabled = algorithm->enabled;
}

CollectivePhase::CollectivePhase() {
    queue_id = -1;
    sys = nullptr;
    algorithm = nullptr;
}

void CollectivePhase::init(BaseStream* stream) {
    if (algorithm != nullptr) {
        algorithm->init(stream);
    }
}
