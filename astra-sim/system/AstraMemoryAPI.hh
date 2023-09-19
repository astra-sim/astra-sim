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

enum TensorLocationType { LOCAL_MEMORY = 0, REMOTE_MEMORY };

class AstraMemoryAPI {
 public:
  virtual ~AstraMemoryAPI() = default;
  virtual void set_sys(int id, Sys* sys) = 0;
  virtual void issue(
      TensorLocationType tensor_loc,
      uint64_t tensor_size,
      WorkloadLayerHandlerData* wlhd) = 0;
  virtual uint64_t get_local_mem_runtime(uint64_t tensor_size) = 0;
};

} // namespace AstraSim

#endif /* __ASTRA_MEMORY_API_HH__ */
