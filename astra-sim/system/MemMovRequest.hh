/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __MEM_MOV_REQUEST_HH__
#define __MEM_MOV_REQUEST_HH__

#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/SharedBusStat.hh"
#include "astra-sim/system/Sys.hh"

namespace AstraSim {

class LogGP;
class MemMovRequest : public Callable, public SharedBusStat {
 public:
  MemMovRequest(
      int request_num,
      Sys* sys,
      LogGP* loggp,
      int size,
      int latency,
      Callable* callable,
      bool processed,
      bool send_back);
  void wait_wait_for_mem_bus(std::list<MemMovRequest>::iterator pointer);
  void set_iterator(std::list<MemMovRequest>::iterator pointer) {
    this->pointer = pointer;
  }
  void call(EventType event, CallData* data);

  static int id;
  int my_id;
  int size;
  int latency;
  Callable* callable;
  bool processed;
  bool send_back;
  bool mem_bus_finished;
  Sys* sys;
  EventType callEvent = EventType::General;
  LogGP* loggp;
  std::list<MemMovRequest>::iterator pointer;

  Tick total_transfer_queue_time;
  Tick total_transfer_time;
  Tick total_processing_queue_time;
  Tick total_processing_time;
  Tick start_time;
  int request_num;
};

} // namespace AstraSim

#endif /* __MEM_MOV_REQUEST_HH__ */
