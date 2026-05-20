/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/Event.h"
#include <cassert>

using namespace NetworkAnalytical;

Event::Event(const Callback callback, const CallbackArg callback_arg) noexcept
    : callback(callback),
      callback_arg(callback_arg) {
    assert(callback != nullptr);
}

void Event::invoke_event() noexcept {
    // check the validity of the event
    assert(callback != nullptr);

    // invoke the callback function
    (*callback)(callback_arg);
}

std::pair<Callback, CallbackArg> Event::get_handler_arg() const noexcept {
    // check the validity of the event
    assert(callback != nullptr);

    return {callback, callback_arg};
}
