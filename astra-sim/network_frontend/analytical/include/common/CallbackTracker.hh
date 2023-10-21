/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <map>
#include "common/CallbackTrackerEntry.hh"
#include "common/ChunkIdGenerator.hh"

namespace AstraSimAnalytical {

/**
 * CallbackTracker keeps track of sim_send() and sim_recv() callbacks of each
 * chunk identified by (tag, src, dest, chunk_size, chunk_id) tuple.
 */
class CallbackTracker {
 public:
  /// Key = (tag, src, dest, chunk_size, chunk_id)
  using Key = std::tuple<int, int, int, ChunkSize, int>;
  CallbackTracker() noexcept;

  /**
   * Search for the entry identified by (tag, src, dest, chunk_size, chunk_id)
   * tuple.
   *
   * @param tag tag of the sim_send() or sim_recv() call
   * @param src src NPU ID of the sim_send() or sim_recv() call
   * @param dest dest NPU ID of the sim_send() or sim_recv() call
   * @param chunk_size chunk size of the sim_send() or sim_recv() call
   * @param chunk_id id of the chunk
   * @return the found entry if exists, std::nullopt otherwise
   */
  std::optional<CallbackTrackerEntry*> search_entry(
      int tag,
      int src,
      int dest,
      ChunkSize chunk_size,
      int chunk_id) noexcept;

  /**
   * Create a new entry identified by (tag, src, dest, chunk_size, chunk_id)
   * tuple.
   *
   * @param tag tag of the sim_send() or sim_recv() call
   * @param src src NPU ID of the sim_send() or sim_recv() call
   * @param dest dest NPU ID of the sim_send() or sim_recv() call
   * @param chunk_size chunk size of the sim_send() or sim_recv() call
   * @param chunk_id id of the chunk
   * @return the created entry
   */
  CallbackTrackerEntry* create_new_entry(
      int tag,
      int src,
      int dest,
      ChunkSize chunk_size,
      int chunk_id) noexcept;

  /**
   * Remove the entry identified by (tag, src, dest, chunk_size, chunk_id)
   * tuple.
   *
   * @param tag tag of the sim_send() or sim_recv() call
   * @param src src NPU ID of the sim_send() or sim_recv() call
   * @param dest dest NPU ID of the sim_send() or sim_recv() call
   * @param chunk_size chunk size of the sim_send() or sim_recv() call
   * @param chunk_id id of the chunk
   */
  void pop_entry(
      int tag,
      int src,
      int dest,
      ChunkSize chunk_size,
      int chunk_id) noexcept;

 private:
  /// map from (tag, src, dest, chunk_size, chunk_id) tuple to
  /// CallbackTrackerEntry
  std::map<Key, CallbackTrackerEntry> tracker;
};

} // namespace AstraSimAnalytical
