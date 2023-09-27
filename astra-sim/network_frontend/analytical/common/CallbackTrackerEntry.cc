/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/CallbackTrackerEntry.hh"
#include <cassert>

using namespace NetworkAnalytical;
using namespace AstraSimAnalytical;

CallbackTrackerEntry::CallbackTrackerEntry() noexcept
    : send_event(std::nullopt),
      recv_event(std::nullopt),
      transmission_finished(false) {}

void CallbackTrackerEntry::register_send_callback(
    const Callback callback,
    const CallbackArg arg) noexcept {
  assert(!send_event.has_value());

  const auto event = Event(callback, arg);
  send_event = event;
}

void CallbackTrackerEntry::register_recv_callback(
    const Callback callback,
    const CallbackArg arg) noexcept {
  assert(!recv_event.has_value());

  const auto event = Event(callback, arg);
  recv_event = event;
}

bool CallbackTrackerEntry::is_transmission_finished() const noexcept {
  return transmission_finished;
}

void CallbackTrackerEntry::set_transmission_finished() noexcept {
  transmission_finished = true;
}

bool CallbackTrackerEntry::both_callbacks_registered() const noexcept {
  return (send_event.has_value() && recv_event.has_value());
}

void CallbackTrackerEntry::invoke_send_handler() noexcept {
  assert(send_event.has_value());

  send_event.value().invoke_event();
}

void CallbackTrackerEntry::invoke_recv_handler() noexcept {
  assert(recv_event.has_value());

  recv_event.value().invoke_event();
}
