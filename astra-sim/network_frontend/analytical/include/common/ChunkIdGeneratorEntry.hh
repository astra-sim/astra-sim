/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

namespace AstraSimAnalytical {

class ChunkIdGeneratorEntry {
 public:
  ChunkIdGeneratorEntry() noexcept;

  [[nodiscard]] int get_send_id() const noexcept;

  [[nodiscard]] int get_recv_id() const noexcept;

  void increment_send_id() noexcept;

  void increment_recv_id() noexcept;

 private:
  int send_id;
  int recv_id;
};

} // namespace AstraSimAnalytical
