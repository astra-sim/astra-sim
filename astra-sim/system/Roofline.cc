/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/Roofline.hh"

#include <algorithm>

using namespace std;
using namespace AstraSim;

Roofline::Roofline(double peak_perf)
  : peak_perf(peak_perf) {
}

Roofline::Roofline(double bandwidth, double peak_perf)
  : bandwidth(bandwidth), peak_perf(peak_perf) {
}

void Roofline::set_bandwidth(double bandwidth) {
  this->bandwidth = bandwidth;
}

double Roofline::get_perf(double operational_intensity) {
  return min(bandwidth * operational_intensity, peak_perf);
}
