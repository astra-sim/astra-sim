/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/basic_system/basic_model/Usage.hh"

using namespace AstraSim;

Usage::Usage(int level, uint64_t start, uint64_t end) {
    this->level = level;
    this->start = start;
    this->end = end;
}
