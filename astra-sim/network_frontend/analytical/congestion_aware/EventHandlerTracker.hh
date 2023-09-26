/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <network_backend/analytical/common/Common.hh>
#include "network_frontend/analytical/congestion_aware/ChunkIdGenerator.hh"
#include "network_frontend/analytical/congestion_aware/EventHandlerTrackerEntry.hh"

namespace AstraSimAnalyticalCongestionAware {

class EventHandlerTracker {
 public:
  using PayloadSize = ChunkIdGenerator::PayloadSize;
  using Key = std::tuple<int, int, int, PayloadSize, int>; // tag, src, dest,
                                                           // count, chunkId
  EventHandlerTracker() noexcept;

  std::optional<EventHandlerTrackerEntry*> search_entry(
      int tag,
      int src,
      int dest,
      PayloadSize count,
      int chunk_id) noexcept;

  EventHandlerTrackerEntry* create_new_entry(
      int tag,
      int src,
      int dest,
      PayloadSize count,
      int chunk_id) noexcept;

  void pop_entry(
      int tag,
      int src,
      int dest,
      PayloadSize count,
      int chunk_id) noexcept;

 private:
  std::map<Key, EventHandlerTrackerEntry> tracker;
};

} // namespace AstraSimAnalyticalCongestionAware
