/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <map>
#include "common/CallbackTrackerEntry.hh"
#include "common/ChunkIdGenerator.hh"

namespace AstraSimAnalytical {

class CallbackTracker {
 public:
  using Key = std::tuple<int, int, int, ChunkSize, int>; // tag, src, dest,
                                                         // chunk_size, chunkId
  CallbackTracker() noexcept;

  std::optional<CallbackTrackerEntry*> search_entry(
      int tag,
      int src,
      int dest,
      ChunkSize chunk_size,
      int chunk_id) noexcept;

  CallbackTrackerEntry* create_new_entry(
      int tag,
      int src,
      int dest,
      ChunkSize chunk_size,
      int chunk_id) noexcept;

  void pop_entry(
      int tag,
      int src,
      int dest,
      ChunkSize chunk_size,
      int chunk_id) noexcept;

 private:
  std::map<Key, CallbackTrackerEntry> tracker;
};

} // namespace AstraSimAnalytical
