/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "SendRecvId.hh"

using namespace Congestion;

SendRecvId::SendRecvId() noexcept : send_id(-1), recv_id(-1) {}

SendRecvId::~SendRecvId() noexcept = default;

int SendRecvId::get_send_id() const noexcept {
  return send_id;
}

void SendRecvId::increase_send_id() noexcept {
  send_id++;
}

int SendRecvId::get_recv_id() const noexcept {
  return recv_id;
}

void SendRecvId::increase_recv_id() noexcept {
  recv_id++;
}
