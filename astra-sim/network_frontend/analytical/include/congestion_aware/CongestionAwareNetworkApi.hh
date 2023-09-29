/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <astra-network-analytical/congestion_aware/Topology.hh>
#include "common/CommonNetworkApi.hh"

using namespace AstraSim;
using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionAware;

namespace AstraSimAnalyticalCongestionAware {

class CongestionAwareNetworkApi final : public CommonNetworkApi {
 public:
  static void set_topology(std::shared_ptr<Topology> topology_ptr) noexcept;

  explicit CongestionAwareNetworkApi(int rank) noexcept;

  int sim_send(
      void* buffer,
      uint64_t count,
      int type,
      int dst,
      int tag,
      sim_request* request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg) override;

 private:
  static std::shared_ptr<Topology> topology;
};

} // namespace AstraSimAnalyticalCongestionAware
