// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

namespace AstraSim {

class IntData : public CallData {
  public:
    IntData(int d) {
        data = d;
    }
    int data;
    uint64_t execution_time;
};

}  // namespace AstraSim
