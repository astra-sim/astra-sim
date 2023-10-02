/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

namespace Congestion {

class SendRecvId {
 public:
  SendRecvId() noexcept;

  ~SendRecvId() noexcept;

  int get_send_id() const noexcept;

  void increase_send_id() noexcept;

  int get_recv_id() const noexcept;

  void increase_recv_id() noexcept;

 private:
  int send_id;
  int recv_id;
};

} // namespace Congestion
