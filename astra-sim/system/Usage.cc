/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/


#include "Usage.hh"
namespace AstraSim {
Usage::Usage(int level, Tick start, Tick end) {
    this->level=level;
    this->start=start;
    this->end=end;
}
} // namespace AstraSim