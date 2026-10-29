// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <algorithm>
#include <random>

namespace AstraSim {
extern std::mt19937 g_rand_generator;
extern std::uniform_int_distribution<uint64_t> g_int_rand_dist;
}  // namespace AstraSim
