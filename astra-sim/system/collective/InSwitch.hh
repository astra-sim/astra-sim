//
// Created by Saeed Rashidi on 2/21/23.
//

#ifndef ASTRA_SIM_DEV_INTERNAL_INSWITCH_H
#define ASTRA_SIM_DEV_INTERNAL_INSWITCH_H

#include "astra-sim/system/collective/Algorithm.hh"
#include "astra-sim/system/CallData.hh"
#include "astra-sim/system/topology/Switch.hh"

namespace AstraSim {

class InSwitch: public Algorithm{
 public:
  enum class State{npu_sending,npu_receiving,switch_receiving,switch_processing,switch_npu_receiving};
    int id;
    Switch *sw;
    State state;
    int num_msg_received;
    uint64_t data_size;
    ComType type;
    InSwitch(int id, Switch *sw, uint64_t size, ComType type);
    void run(EventType event, CallData* data);
    void run_all_reduce(EventType event, CallData* data);
    void run_reduce_scatter(EventType event, CallData* data);
    void run_all_gather(EventType event, CallData* data);
    void run_all_to_all(EventType event, CallData* data);

};

}
#endif // ASTRA_SIM_DEV_INTERNAL_INSWITCH_H
