/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __COLLECTIVE_PHASE_HH__
#define __COLLECTIVE_PHASE_HH__

#include "astra-sim/system/Common.hh"

namespace AstraSim {

class Sys;
class Algorithm;
class BaseStream;

class CollectivePhase {
 public:
  CollectivePhase(Sys* sys, int queue_id, Algorithm* algorithm);
  CollectivePhase();
  void init(BaseStream* stream);

  Sys* sys;
  int queue_id;
  Algorithm* algorithm;
  uint64_t initial_data_size;
  uint64_t final_data_size;
  bool enabled;
  ComType comm_type;
};

} // namespace AstraSim

#endif /* __COLLECTIVE_PHASE_HH__ */
