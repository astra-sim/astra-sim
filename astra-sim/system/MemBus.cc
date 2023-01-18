/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/MemBus.hh"

#include "astra-sim/system/LogGP.hh"
#include "astra-sim/system/Sys.hh"

using namespace std;
using namespace AstraSim;

MemBus::MemBus(
    string side1,
    string side2,
    Sys* sys,
    Tick L,
    Tick o,
    Tick g,
    double G,
    bool model_shared_bus,
    int communication_delay,
    bool attach) {
  NPU_side = new LogGP(side1, sys, L, o, g, G, EventType::MA_to_NPU);
  MA_side = new LogGP(side2, sys, L, o, g, G, EventType::NPU_to_MA);
  NPU_side->partner = MA_side;
  MA_side->partner = NPU_side;
  this->sys = sys;
  this->model_shared_bus = model_shared_bus;
  this->communication_delay = communication_delay;
  if (attach) {
    NPU_side->attach_mem_bus(
        sys, L, o, g, 0.0038, model_shared_bus, communication_delay);
  }
}

MemBus::~MemBus() {
  delete NPU_side;
  delete MA_side;
}

void MemBus::send_from_NPU_to_MA(
    MemBus::Transmition transmition,
    int bytes,
    bool processed,
    bool send_back,
    Callable* callable) {
  if (model_shared_bus && transmition == Transmition::Usual) {
    NPU_side->request_read(bytes, processed, send_back, callable);
  } else {
    if (transmition == Transmition::Fast) {
      SharedBusStat* ss = new SharedBusStat(BusType::Shared, 0, 10, 0, 0);
      ss->sys_id = sys->id;
      ss->event = EventType::NPU_to_MA;
      sys->register_event(
          callable,
          EventType::NPU_to_MA,
          ss,
          10);
    } else {
      SharedBusStat* ss = new SharedBusStat(BusType::Shared, 0, communication_delay, 0, 0);
      ss->sys_id = sys->id;
      ss->event = EventType::NPU_to_MA;
      sys->register_event(
          callable,
          EventType::NPU_to_MA,
          ss,
          communication_delay);
    }
  }
}

void MemBus::send_from_MA_to_NPU(
    MemBus::Transmition transmition,
    int bytes,
    bool processed,
    bool send_back,
    Callable* callable) {
  if (model_shared_bus && transmition == Transmition::Usual) {
    MA_side->request_read(bytes, processed, send_back, callable);
  } else {
    if (transmition == Transmition::Fast) {
      SharedBusStat* ss = new SharedBusStat(BusType::Shared, 0, 10, 0, 0);
      ss->sys_id = sys->id;
      ss->event = EventType::MA_to_NPU;
      sys->register_event(
          callable,
          EventType::MA_to_NPU,
          ss,
          10);
    } else {
      SharedBusStat* ss = new SharedBusStat(BusType::Shared, 0, communication_delay, 0, 0);
      ss->sys_id = sys->id;
      ss->event = EventType::MA_to_NPU;
      sys->register_event(
          callable,
          EventType::MA_to_NPU,
          ss,
          communication_delay);
    }
  }
}
