// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/common/CallData.hh>

namespace AstraSim {

class BasicEventHandlerData : public CallData {
  public:
    BasicEventHandlerData();
    BasicEventHandlerData(int sys_id, EventType event);

    int sys_id;
    EventType event;
};

}  // namespace AstraSim
