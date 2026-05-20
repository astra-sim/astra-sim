/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include <list>
#include <memory>

namespace NetworkAnalyticalCongestionAware {

/// Forward declarations of network components
class Chunk;
class Link;
class Device;

/// Route is a list of devices
using Route = std::list<std::shared_ptr<Device>>;

}  // namespace NetworkAnalyticalCongestionAware
