/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <optional>
#include "event-queue/Event.hh"
#include "type/Type.hh"

namespace Congestion {

class EventHandlerTrackerEntry {
 public:
  EventHandlerTrackerEntry() noexcept;

  ~EventHandlerTrackerEntry() noexcept;

  void register_send_callback(Callback callback, CallbackArg arg) noexcept;

  void register_recv_callback(Callback callback, CallbackArg arg) noexcept;

  bool is_transmission_finished() const noexcept;

  bool both_callbacks_registered() const noexcept;

  void set_transmission_finished() noexcept;

  void invoke_send_handler() noexcept;

  void invoke_recv_handler() noexcept;

 private:
  std::optional<Event> send_event;
  std::optional<Event> recv_event;

  bool transmission_finished;
};

} // namespace Congestion
