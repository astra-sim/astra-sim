// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/common/BasicEventHandlerData.hh>

using namespace AstraSim;

BasicEventHandlerData::BasicEventHandlerData() {
    this->sys_id = 0;
}

BasicEventHandlerData::BasicEventHandlerData(int sys_id, EventType event) {
    this->sys_id = sys_id;
    this->event = event;
}
