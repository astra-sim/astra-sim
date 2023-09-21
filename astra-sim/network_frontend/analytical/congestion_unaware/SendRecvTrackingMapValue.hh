/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __SEND_RECV_TRACKING_MAP_VALUE_HH__
#define __SEND_RECV_TRACKING_MAP_VALUE_HH__

#include <astra-sim/system/AstraNetworkAPI.hh>
#include <congestion_unaware/event-queue/Event.hh>

namespace Analytical {
class SendRecvTrackingMapValue {
 public:
  /**
   * Constructor for send operation
   * @param send_finish_time send operation finish time
   * @return instance with send operation set
   */
  static SendRecvTrackingMapValue make_send_value(
      AstraSim::timespec_t send_finish_time) noexcept;

  /**
   * Constructor for recv operation
   * @param fun_ptr recv event handler
   * @param fun_arg recv event handler argument
   * @return instance with recv operation set
   */
  static SendRecvTrackingMapValue make_recv_value(
      void (*fun_ptr)(void*),
      void* fun_arg) noexcept;

  /**
   * Check whether the operation is send.
   * @return true if operation is send, false if operation is recv
   */
  bool is_send() const noexcept;

  /**
   * Check whether the operation is recv.
   * @return true if operation is recv, false if operation is send
   */
  bool is_recv() const noexcept;

  /**
   * send_finish_time getter
   * @return send_finish_timme
   */
  AstraSim::timespec_t get_send_finish_time() const noexcept;

  /**
   * recv_event getter
   * @return recv_event
   */
  Event get_recv_event() const noexcept;

 private:
  enum class OperationType { send, recv };
  OperationType operation_type;

  /**
   * For send operation: mark when send should finish
   * Note: this is an actual time of event_queue, not delta
   */
  AstraSim::timespec_t send_finish_time;

  /**
   * For recv operation: save receive event handler
   */
  Event recv_event;

  /**
   * Hidden constructor
   * @param operation_type send/recv event type
   * @param send_finish_time for send -- send operation ending time
   * @param fun_ptr for recv operation -- recv event handler
   * @param fun_arg for recv operation -- recv event handler argument
   */
  SendRecvTrackingMapValue(
      OperationType operation_type,
      AstraSim::timespec_t send_finish_time,
      void (*fun_ptr)(void*),
      void* fun_arg) noexcept
      : operation_type(operation_type),
        send_finish_time(send_finish_time),
        recv_event(fun_ptr, fun_arg) {}
};
} // namespace Analytical

#endif /* __SEND_RECV_TRACKING_MAP_VALUE_HH__ */
