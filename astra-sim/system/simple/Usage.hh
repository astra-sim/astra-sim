// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cstdint>

namespace AstraSim {

class Usage {
  public:
    int level;
    uint64_t start;
    uint64_t end;
    Usage(int level, uint64_t start, uint64_t end);
};

}  // namespace AstraSim
