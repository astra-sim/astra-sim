/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <network_backend/analytical/common/Common.hh>
#include "SendRecvId.hh"

namespace AstraSimAnalyticalCongestionAware {

class ChunkIdGenerator {
 public:
  using PayloadSize = uint64_t;
  using Key = std::tuple<int, int, int, PayloadSize>; // tag, src, dest, count

  ChunkIdGenerator() noexcept;

  int get_send_id(int tag, int src, int dest, PayloadSize count) noexcept;

  int get_recv_id(int tag, int src, int dest, PayloadSize count) noexcept;

 private:
  std::map<Key, SendRecvId> chunk_id_map;
};

} // namespace AstraSimAnalyticalCongestionAware
