// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/astraccl/Algorithm.hh>

#include <astra-sim/system/simple/SimpleSys.hh>

using namespace AstraSim;

Algorithm::Algorithm() {
    enabled = true;
}

void Algorithm::init(BaseStream* stream) {
    this->stream = stream;
}

void Algorithm::call(EventType event, CallData* data) {}

void Algorithm::exit() {
    stream->owner->proceed_to_next_vnet_baseline((StreamBaseline*)stream);
}
