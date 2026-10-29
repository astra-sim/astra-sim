// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/common/Random.hh>

namespace AstraSim {
std::mt19937 g_rand_generator(0);
std::uniform_int_distribution<uint64_t> g_int_rand_dist;
}  // namespace AstraSim
