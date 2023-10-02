/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "SendRecvTrackingMapValue.hh"

Analytical::SendRecvTrackingMapValue Analytical::SendRecvTrackingMapValue::
    make_send_value(AstraSim::timespec_t send_finish_time) noexcept {
  return {OperationType::send, send_finish_time, nullptr, nullptr};
}

Analytical::SendRecvTrackingMapValue Analytical::SendRecvTrackingMapValue::
    make_recv_value(void (*fun_ptr)(void*), void* fun_arg) noexcept {
  return {OperationType::recv, AstraSim::timespec_t(), fun_ptr, fun_arg};
}

bool Analytical::SendRecvTrackingMapValue::is_send() const noexcept {
  return operation_type == OperationType::send;
}

bool Analytical::SendRecvTrackingMapValue::is_recv() const noexcept {
  return operation_type == OperationType::recv;
}

AstraSim::timespec_t Analytical::SendRecvTrackingMapValue::
    get_send_finish_time() const noexcept {
  return send_finish_time;
}

Analytical::Event Analytical::SendRecvTrackingMapValue::get_recv_event()
    const noexcept {
  return recv_event;
}
