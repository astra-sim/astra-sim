/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <astra-network-analytical/common/Event.hh>
#include <astra-network-analytical/common/Type.hh>
#include <optional>

using namespace NetworkAnalytical;

namespace AstraSimAnalytical {

class CallbackTrackerEntry {
 public:
  CallbackTrackerEntry() noexcept;

  void register_send_callback(Callback callback, CallbackArg arg) noexcept;

  void register_recv_callback(Callback callback, CallbackArg arg) noexcept;

  [[nodiscard]] bool is_transmission_finished() const noexcept;

  void set_transmission_finished() noexcept;

  [[nodiscard]] bool both_callbacks_registered() const noexcept;

  void invoke_send_handler() noexcept;

  void invoke_recv_handler() noexcept;

  [[nodiscard]] Event get_send_event() const noexcept;

  [[nodiscard]] Event get_recv_event() const noexcept;

 private:
  std::optional<Event> send_event;
  std::optional<Event> recv_event;
  bool transmission_finished;
};

} // namespace AstraSimAnalytical
