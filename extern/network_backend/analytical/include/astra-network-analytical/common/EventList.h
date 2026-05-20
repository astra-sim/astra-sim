/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "common/Event.h"
#include "common/Type.h"
#include <list>

namespace NetworkAnalytical {

/**
 * EventList encapsulates a number of Events along with its event time.
 */
class EventList {
  public:
    /**
     * Constructor.
     *
     * @param event_time event time of the event list
     */
    explicit EventList(EventTime event_time) noexcept;

    /**
     * Get the registered event time.
     *
     * @return event time
     */
    [[nodiscard]] EventTime get_event_time() const noexcept;

    /**
     * Register an event into the event list.
     *
     * @param callback callback function pointer
     * @param callback_arg argument of the callback function
     */
    void add_event(Callback callback, CallbackArg callback_arg) noexcept;

    /**
     * Invoke all events in the event list.
     */
    void invoke_events() noexcept;

  private:
    /// event time of the event list
    EventTime event_time;

    /// list of registered events
    std::list<Event> events;
};

}  // namespace NetworkAnalytical
