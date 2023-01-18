/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __MY_PACKET_HH__
#define __MY_PACKET_HH__

#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/Common.hh"

namespace AstraSim {

class MyPacket : public Callable {
 public:
  MyPacket(
      int preferred_vnet,
      int preferred_src,
      int preferred_dest);
  MyPacket(
      uint64_t msg_size,
      int preferred_vnet,
      int preferred_src,
      int preferred_dest);
  void set_notifier(Callable* c);
  void call(EventType event, CallData* data);

  int cycles_needed;
  int fm_id;
  int stream_id;
  Callable* notifier;
  Callable* sender;
  int preferred_vnet;
  int preferred_dest;
  int preferred_src;
  uint64_t msg_size;
  Tick ready_time;
};

} // namespace AstraSim

#endif /* __MY_PACKET_HH__ */
