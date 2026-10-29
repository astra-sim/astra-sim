// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

namespace AstraSim {

class Roofline {
  public:
    Roofline(double peak_perf);
    Roofline(double bandwidth, double peak_perf);
    void set_bandwidth(double bandwidth);
    double get_perf(double operational_intensity);

  private:
    double bandwidth;
    double peak_perf;
};

}  // namespace AstraSim
