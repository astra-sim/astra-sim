/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once

#include "common/Type.h"
#include "congestion_aware/Type.h"
#include <map>
#include <memory>

using namespace NetworkAnalytical;

namespace NetworkAnalyticalCongestionAware {

/**
 * Device class represents a single device in the network.
 * Device is usually an NPU or a switch.
 */
class Device {
  public:
    /**
     * Constructor.
     *
     * @param id id of the device
     */
    explicit Device(DeviceId id) noexcept;

    /**
     * Get id of the device.
     *
     * @return id of the device
     */
    [[nodiscard]] DeviceId get_id() const noexcept;

    /**
     * Initiate a chunk transmission.
     * You must invoke this method on the source device of the chunk.
     *
     * @param chunk chunk to send
     */
    void send(std::unique_ptr<Chunk> chunk) noexcept;

    /**
     * Connect a device to another device.
     *
     * @param id id of the device to connect this device to
     * @param bandwidth bandwidth of the link
     * @param latency latency of the link
     */
    void connect(DeviceId id, Bandwidth bandwidth, Latency latency) noexcept;

  private:
    /// device Id
    DeviceId device_id;

    /// links to other nodes
    /// map[dest node node_id] -> link
    std::map<DeviceId, std::shared_ptr<Link>> links;

    /**
     * Check if this device is connected to another device.
     *
     * @param dest id of the device to check te connectivity
     * @return true if connected to the given device, false otherwise
     */
    [[nodiscard]] bool connected(DeviceId dest) const noexcept;
};

}  // namespace NetworkAnalyticalCongestionAware
