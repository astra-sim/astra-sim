/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "network_frontend/analytical/congestion_aware/EventHandlerTracker.hh"

using namespace AstraSimAnalyticalCongestionAware;

EventHandlerTracker::EventHandlerTracker() noexcept {
  tracker = {};
}

std::optional<EventHandlerTrackerEntry*> EventHandlerTracker::search_entry(
    const int tag,
    const int src,
    const int dest,
    const PayloadSize count,
    const int chunk_id) noexcept {
  assert(tag >= 0);
  assert(src >= 0);
  assert(dest >= 0);
  assert(count > 0);
  assert(chunk_id >= 0);

  const auto key = std::make_tuple(tag, src, dest, count, chunk_id);
  const auto entry = tracker.find(key);

  if (entry == tracker.end()) {
    // no entry exists
    return std::nullopt;
  }

  // return reference to entry
  return &entry->second;
}

EventHandlerTrackerEntry* EventHandlerTracker::create_new_entry(
    const int tag,
    const int src,
    const int dest,
    const PayloadSize count,
    const int chunk_id) noexcept {
  assert(tag >= 0);
  assert(src >= 0);
  assert(dest >= 0);
  assert(count > 0);
  assert(chunk_id >= 0);

  const auto key = std::make_tuple(tag, src, dest, count, chunk_id);
  const auto entry = tracker.emplace(key, EventHandlerTrackerEntry());
  return &entry.first->second;
}

void EventHandlerTracker::pop_entry(
    const int tag,
    const int src,
    const int dest,
    const PayloadSize count,
    const int chunk_id) noexcept {
  assert(tag >= 0);
  assert(src >= 0);
  assert(dest >= 0);
  assert(count > 0);
  assert(chunk_id >= 0);

  const auto key = std::make_tuple(tag, src, dest, count, chunk_id);
  const auto entry = tracker.find(key);
  tracker.erase(entry);
}
