/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ALL_TO_ALL_HH__
#define __ALL_TO_ALL_HH__

#include "astra-sim/system/basic_system/astraccl/native_collectives/collective_algorithm/Ring.hh"
#include "astra-sim/system/basic_system/astraccl/native_collectives/logical_topology/RingTopology.hh"
#include "astra-sim/system/basic_system/basic_model/CallData.hh"

namespace AstraSim {

class AllToAll : public Ring {
  public:
    AllToAll(ComType type,
             int window,
             int id,
             RingTopology* allToAllTopology,
             uint64_t data_size,
             RingTopology::Direction direction,
             InjectionPolicy injection_policy);
    void run(EventType event, CallData* data);
    void process_max_count();
    int get_non_zero_latency_packets();
    int middle_point;
};

}  // namespace AstraSim

#endif /* __ALL_TO_ALL_HH__ */
