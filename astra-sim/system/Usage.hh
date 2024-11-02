/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __USAGE_HH__
#define __USAGE_HH__

#include <cstdint>

namespace AstraSim {

class Usage {
 public:
  int level;
  uint64_t start;
  uint64_t end;
  Usage(int level, uint64_t start, uint64_t end);
};

} // namespace AstraSim

#endif /* __USAGE_HH__ */
