// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/Usage.hh>

using namespace AstraSim;

Usage::Usage(int level, uint64_t start, uint64_t end) {
    this->level = level;
    this->start = start;
    this->end = end;
}
