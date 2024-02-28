/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ASTRA_MEMORY_API_HH__
#define __ASTRA_MEMORY_API_HH__

#include <cstdint>

namespace AstraSim {

class Sys;
class WorkloadLayerHandlerData;

class AstraRemoteMemoryAPI {
 public:
  virtual ~AstraRemoteMemoryAPI() = default;
  virtual void set_sys(int id, Sys* sys) = 0;
  virtual void issue(uint64_t tensor_size, WorkloadLayerHandlerData* wlhd) = 0;
};

} // namespace AstraSim

#endif /* __ASTRA_MEMORY_API_HH__ */
