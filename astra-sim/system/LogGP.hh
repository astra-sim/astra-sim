/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __LOGGP_HH__
#define __LOGGP_HH__

#include <list>

#include "astra-sim/system/Common.hh"
#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/MemBus.hh"
#include "astra-sim/system/MemMovRequest.hh"

namespace AstraSim {

class Sys;
class LogGP : public Callable {
 public:
  LogGP(
      std::string name,
      Sys* sys,
      Tick L,
      Tick o,
      Tick g,
      double G,
      EventType trigger_event);
  ~LogGP();
  void process_next_read();
  void request_read(
      int bytes,
      bool processed,
      bool send_back,
      Callable* callable);
  void switch_to_receiver(MemMovRequest mr, Tick offset);
  void call(EventType event, CallData* data);
  void attach_mem_bus(
      Sys* sys,
      Tick L,
      Tick o,
      Tick g,
      double G,
      bool model_shared_bus,
      int communication_delay);

  enum class State { Free, waiting, Sending, Receiving };
  enum class ProcState { Free, Processing };

  MemBus* NPU_MEM;
  int request_num;
  std::string name;
  Tick L;
  Tick o;
  Tick g;
  double G;
  Tick last_trans;
  State curState;
  State prevState;
  ProcState processing_state;
  std::list<MemMovRequest> sends;
  std::list<MemMovRequest> receives;
  std::list<MemMovRequest> processing;

  std::list<MemMovRequest> retirements;
  std::list<MemMovRequest> pre_send;
  std::list<MemMovRequest> pre_process;
  std::list<MemMovRequest>::iterator talking_it;

  LogGP* partner;
  Sys* sys;
  EventType trigger_event;
  int subsequent_reads;
  int THRESHOLD;
  int local_reduction_delay;
};

} // namespace AstraSim

#endif /* __LOGGP_HH__ */
