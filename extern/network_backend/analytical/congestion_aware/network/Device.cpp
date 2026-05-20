/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "congestion_aware/Device.h"
#include "congestion_aware/Chunk.h"
#include "congestion_aware/Link.h"
#include <cassert>

using namespace NetworkAnalyticalCongestionAware;

Device::Device(const DeviceId id) noexcept : device_id(id) {
    assert(id >= 0);
}

DeviceId Device::get_id() const noexcept {
    assert(device_id >= 0);

    return device_id;
}

void Device::send(std::unique_ptr<Chunk> chunk) noexcept {
    // assert the validity of the chunk
    assert(chunk != nullptr);

    // assert this node is the current source of the chunk
    assert(chunk->current_device()->get_id() == device_id);

    // assert the chunk hasn't arrived its final destination yet
    assert(!chunk->arrived_dest());

    // get next dest
    const auto next_dest = chunk->next_device();
    const auto next_dest_id = next_dest->get_id();

    // assert the next dest is connected to this node
    assert(connected(next_dest_id));

    // send the chunk to the next dest
    // delegate this task to the link
    links[next_dest_id]->send(std::move(chunk));
}

void Device::connect(const DeviceId id, const Bandwidth bandwidth, const Latency latency) noexcept {
    assert(id >= 0);
    assert(bandwidth > 0);
    assert(latency >= 0);

    // assert there's no existing connection
    assert(!connected(id));

    // create link
    links[id] = std::make_shared<Link>(bandwidth, latency);
}

bool Device::connected(const DeviceId dest) const noexcept {
    assert(dest >= 0);

    // check whether the connection exists
    return links.find(dest) != links.end();
}
