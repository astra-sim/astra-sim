/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __SEND_RECV_TRACKING_MAP_HH__
#define __SEND_RECV_TRACKING_MAP_HH__

#include <map>
#include <tuple>

#include <astra-sim/system/AstraNetworkAPI.hh>
#include <congestion_unaware/topology/TopologyConfig.hh>
#include "SendRecvTrackingMapValue.hh"

namespace Analytical {
class SendRecvTrackingMap {
 public:
  using PayloadSize = TopologyConfig::PayloadSize;

  /**
   * Check whether send operation with given key entry exists.
   * @param tag
   * @param src
   * @param dest
   * @param count
   * @return true if send operation exists, false if not
   */
  bool has_send_operation(int tag, int src, int dest, PayloadSize count)
      const noexcept;

  /**
   * Check whether recv operation with given key entry exists.
   * @param tag
   * @param src
   * @param dest
   * @param count
   * @return true if recv operation exists, false if not
   */
  bool has_recv_operation(int tag, int src, int dest, PayloadSize count)
      const noexcept;

  /**
   * Remove the send operation entry with given key, and return send_finish_time
   * value. Assertion: send_operation with given key must exist.
   * @param tag
   * @param src
   * @param dest
   * @param count
   * @return send_finish_time value
   */
  AstraSim::timespec_t pop_send_finish_time(
      int tag,
      int src,
      int dest,
      PayloadSize count) noexcept;

  /**
   * Remove the recv operation entry with given key, and return
   * recv_event_handler value. Assertion: recv operation with given key must
   * exist.
   * @param tag
   * @param src
   * @param dest
   * @param count
   * @return recv_event_handler
   */
  Event pop_recv_event_handler(
      int tag,
      int src,
      int dest,
      PayloadSize count) noexcept;

  /**
   * Insert a new send operation with given key.
   *      Assertion: given key entry should not exist in the map
   * @param tag
   * @param src
   * @param dest
   * @param count
   * @param send_finish_time send_finish_time to write into the table
   */
  void insert_send(
      int tag,
      int src,
      int dest,
      PayloadSize count,
      AstraSim::timespec_t send_finish_time) noexcept;

  /**
   * Insert a new recv operation with given key.
   *      Assertion: given key entry should not exist in the map.
   * @param tag
   * @param src
   * @param dest
   * @param count
   * @param fun_ptr recv event handler
   * @param fun_arg recv event handler argument
   */
  void insert_recv(
      int tag,
      int src,
      int dest,
      PayloadSize count,
      void (*fun_ptr)(void*),
      void* fun_arg) noexcept;

  /**
   * For debugging purpose: print status for debugging.
   */
  void print() const noexcept;

 private:
  /**
   * tag, src, dest, count
   */
  typedef std::tuple<int, int, int, PayloadSize> Key;

  /**
   * Send and recv tracking map
   * (tag, src, dest, count) -> Value
   */
  std::multimap<Key, SendRecvTrackingMapValue> send_recv_tracking_map;
};
} // namespace Analytical

#endif /* __SEND_RECV_TRACKING_MAP_HH__ */
