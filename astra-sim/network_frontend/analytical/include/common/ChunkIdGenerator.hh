/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <astra-network-analytical/common/Type.hh>
#include <map>
#include <tuple>
#include "common/ChunkIdGeneratorEntry.hh"

using namespace NetworkAnalytical;

namespace AstraSimAnalytical {

class ChunkIdGenerator {
 public:
  using Key = std::tuple<int, int, int, ChunkSize>; // tag, src, dest, count

  ChunkIdGenerator() noexcept;

  [[nodiscard]] int create_send_chunk_id(
      int tag,
      int src,
      int dest,
      ChunkSize chunk_size) noexcept;

  [[nodiscard]] int create_recv_chunk_id(
      int tag,
      int src,
      int dest,
      ChunkSize chunk_size) noexcept;

 private:
  std::map<Key, ChunkIdGeneratorEntry> chunk_id_map;
};

} // namespace AstraSimAnalytical
