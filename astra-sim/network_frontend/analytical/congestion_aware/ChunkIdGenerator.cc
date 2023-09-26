/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "network_frontend/analytical/congestion_aware/ChunkIdGenerator.hh"

using namespace AstraSimAnalyticalCongestionAware;

ChunkIdGenerator::ChunkIdGenerator() noexcept {
  chunk_id_map = std::map<Key, SendRecvId>();
}

int ChunkIdGenerator::get_send_id(
    const int tag,
    const int src,
    const int dest,
    const PayloadSize count) noexcept {
  assert(tag >= 0);
  assert(src >= 0);
  assert(dest >= 0);
  assert(count > 0);

  // create key
  const auto key = std::make_tuple(tag, src, dest, count);

  // search whether the key exists
  auto entry = chunk_id_map.find(key);

  if (entry == chunk_id_map.end()) {
    // key doesn't exist, create new entry
    chunk_id_map[key] = SendRecvId();
    entry = chunk_id_map.insert({key, SendRecvId()}).first;
  }

  // increment id and return
  entry->second.increase_send_id();
  return entry->second.get_send_id();
}

int ChunkIdGenerator::get_recv_id(
    const int tag,
    const int src,
    const int dest,
    const PayloadSize count) noexcept {
  assert(tag >= 0);
  assert(src >= 0);
  assert(dest >= 0);
  assert(count > 0);

  // create key
  const auto key = std::make_tuple(tag, src, dest, count);

  // search whether the key exists
  auto entry = chunk_id_map.find(key);

  if (entry == chunk_id_map.end()) {
    // key doesn't exist, create new entry
    entry = chunk_id_map.insert({key, SendRecvId()}).first;
  }

  // if key exists, increment send id and return
  entry->second.increase_recv_id();
  return entry->second.get_recv_id();
}
