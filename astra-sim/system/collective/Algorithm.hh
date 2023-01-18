/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ALGORITHM_HH__
#define __ALGORITHM_HH__

#include "astra-sim/system/BaseStream.hh"
#include "astra-sim/system/CallData.hh"
#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/topology/LogicalTopology.hh"

namespace AstraSim {

class Algorithm : public Callable {
 public:
  enum class Name {
    Ring = 0,
    DoubleBinaryTree,
    AllToAll,
    HalvingDoubling};

  Algorithm();
  virtual ~Algorithm() = default;
  virtual void run(EventType event, CallData* data) = 0;
  virtual void init(BaseStream* stream);
  virtual void call(EventType event, CallData* data);
  virtual void exit();

  Name name;
  int id;
  BaseStream* stream;
  LogicalTopology* logical_topo;
  uint64_t data_size;
  uint64_t final_data_size;
  ComType comType;
  bool enabled;
};

} // namespace AstraSim

#endif /* __ALGORITHM_HH__ */
