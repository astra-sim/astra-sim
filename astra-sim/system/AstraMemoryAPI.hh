/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ASTRA_MEMORY_API_HH__
#define __ASTRA_MEMORY_API_HH__

#include <cstdint>

namespace AstraSim {

class AstraMemoryAPI {
 public:
  virtual uint64_t mem_read(uint64_t size) = 0;
  virtual uint64_t mem_write(uint64_t size) = 0;
  virtual uint64_t npu_mem_read(uint64_t size) = 0;
  virtual uint64_t npu_mem_write(uint64_t size) = 0;
  virtual uint64_t nic_mem_read(uint64_t size) = 0;
  virtual uint64_t nic_mem_write(uint64_t size) = 0;
  virtual ~AstraMemoryAPI() = default;
};

} // namespace AstraSim

#endif /* __ASTRA_MEMORY_API_HH__ */
