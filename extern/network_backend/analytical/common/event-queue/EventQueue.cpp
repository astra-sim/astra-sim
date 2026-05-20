/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/EventQueue.h"
#include <cassert>

using namespace NetworkAnalytical;

EventQueue::EventQueue() noexcept : current_time(0) {
    // create empty event queue
    event_queue = std::list<EventList>();
}

EventTime EventQueue::get_current_time() const noexcept {
    return current_time;
}

bool EventQueue::finished() const noexcept {
    // check whether event queue is empty
    return event_queue.empty();
}

void EventQueue::proceed() noexcept {
    // to proceed, next event should exist
    assert(!finished());

    // proceed to the next event time
    auto& current_event_list = event_queue.front();

    // check the validity and update current time
    assert(current_event_list.get_event_time() > current_time);
    current_time = current_event_list.get_event_time();

    // invoke events
    current_event_list.invoke_events();

    // drop processed event list
    event_queue.pop_front();
}

void EventQueue::schedule_event(const EventTime event_time,
                                const Callback callback,
                                const CallbackArg callback_arg) noexcept {
    // time should be at least larger than current time
    assert(event_time >= current_time);

    // find the entry to insert event
    auto event_list_it = event_queue.begin();
    while (event_list_it != event_queue.end() && event_list_it->get_event_time() < event_time) {
        event_list_it++;
    }

    // There can be three scenarios:
    // (1) event list matching with event_time is found
    // (2) there's no event list matching with event_time
    //   (2-1) the event_time requested is
    //   larger than the largest event time scheduled
    //   (2-2) the event_time requested is
    //   smaller than the largest event time scheduled
    // for both (2-1) or (2-2), a new event should be created
    if (event_list_it == event_queue.end() || event_time < event_list_it->get_event_time()) {
        // insert new event_list
        event_list_it = event_queue.insert(event_list_it, EventList(event_time));
    }

    // now, whether (1) or (2), the entry to insert the event is found
    // add event to event_list
    event_list_it->add_event(callback, callback_arg);
}
