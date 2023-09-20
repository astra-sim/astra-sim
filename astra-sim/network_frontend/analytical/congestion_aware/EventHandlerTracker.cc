/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "EventHandlerTracker.hh"

using namespace Congestion;

EventHandlerTracker::EventHandlerTracker() noexcept {
  tracker = {};
}

EventHandlerTracker::~EventHandlerTracker() noexcept = default;

std::optional<EventHandlerTrackerEntry*> EventHandlerTracker::search_entry(
    int tag,
    int src,
    int dest,
    PayloadSize count,
    int chunk_id) noexcept {
  auto key = std::make_tuple(tag, src, dest, count, chunk_id);
  auto entry = tracker.find(key);

  if (entry == tracker.end()) {
    // no entry exists
    return std::nullopt;
  }

  // return reference to entry
  return &entry->second;
}

EventHandlerTrackerEntry* EventHandlerTracker::create_new_entry(
    int tag,
    int src,
    int dest,
    PayloadSize count,
    int chunk_id) noexcept {
  auto key = std::make_tuple(tag, src, dest, count, chunk_id);
  auto entry = tracker.emplace(key, EventHandlerTrackerEntry());
  return &entry.first->second;
}

void EventHandlerTracker::pop_entry(
    int tag,
    int src,
    int dest,
    PayloadSize count,
    int chunk_id) noexcept {
  auto key = std::make_tuple(tag, src, dest, count, chunk_id);
  auto entry = tracker.find(key);
  tracker.erase(entry);
}
