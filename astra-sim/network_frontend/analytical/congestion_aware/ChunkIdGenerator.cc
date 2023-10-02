/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "ChunkIdGenerator.hh"

using namespace Congestion;

ChunkIdGenerator::ChunkIdGenerator() noexcept {
  chunk_id_map = std::map<Key, SendRecvId>();
}

ChunkIdGenerator::~ChunkIdGenerator() noexcept = default;

int ChunkIdGenerator::get_send_id(
    int tag,
    int src,
    int dest,
    PayloadSize count) noexcept {
  // create key
  auto key = std::make_tuple(tag, src, dest, count);

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
    int tag,
    int src,
    int dest,
    PayloadSize count) noexcept {
  // create key
  auto key = std::make_tuple(tag, src, dest, count);

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
