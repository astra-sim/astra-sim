/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __SIMSENDCALLER_HH__
#define __SIMSENDCALLER_HH__

#include <assert.h>
#include <math.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <list>
#include <map>
#include <sstream>
#include <tuple>
#include <vector>
#include "CallData.hh"
#include "Callable.hh"
#include "Common.hh"

namespace AstraSim {
class Sys;
class SimSendCaller : public Callable {
 public:
  void* buffer;
  int count;
  int type;
  int dst;
  int tag;
  sim_request request;
  void (*msg_handler)(void* fun_arg);
  void* fun_arg;
  void call(EventType type, CallData* data);
  Sys* generator;
  SimSendCaller(
      Sys* generator,
      void* buffer,
      int count,
      int type,
      int dst,
      int tag,
      sim_request request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg);
};
} // namespace AstraSim
#endif
