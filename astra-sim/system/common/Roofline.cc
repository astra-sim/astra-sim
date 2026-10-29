// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/common/Roofline.hh>

#include <algorithm>

using namespace AstraSim;

Roofline::Roofline(double peak_perf) : peak_perf(peak_perf) {}

Roofline::Roofline(double bandwidth, double peak_perf)
    : bandwidth(bandwidth),
      peak_perf(peak_perf) {}

void Roofline::set_bandwidth(double bandwidth) {
    this->bandwidth = bandwidth;
}

double Roofline::get_perf(double operational_intensity) {
    return std::min(bandwidth * operational_intensity, peak_perf);
}
