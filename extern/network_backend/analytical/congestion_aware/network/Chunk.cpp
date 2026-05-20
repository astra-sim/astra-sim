/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "congestion_aware/Chunk.h"
#include "congestion_aware/Device.h"
#include "congestion_aware/Link.h"
#include <cassert>

using namespace NetworkAnalyticalCongestionAware;

void Chunk::chunk_arrived_next_device(void* const chunk_ptr) noexcept {
    assert(chunk_ptr != nullptr);

    // cast to unique_ptr<Chunk>
    auto chunk = std::unique_ptr<Chunk>(static_cast<Chunk*>(chunk_ptr));

    // mark chunk arrived next node
    chunk->mark_arrived_next_device();

    if (chunk->arrived_dest()) {
        // chunk arrived dest, invoke callback
        // as chunk is unique_ptr, will be destroyed automatically
        chunk->invoke_callback();
    } else {
        // send this chunk to next dest
        const auto current_node = chunk->current_device();
        current_node->send(std::move(chunk));  // send chunk to next des
    }
}

Chunk::Chunk(const ChunkSize chunk_size, Route route, const Callback callback, const CallbackArg callback_arg) noexcept
    : chunk_size(chunk_size),
      route(std::move(route)),
      callback(callback),
      callback_arg(callback_arg) {
    assert(chunk_size > 0);
    assert(!this->route.empty());
    assert(callback != nullptr);
}

std::shared_ptr<Device> Chunk::current_device() const noexcept {
    // assert the route is not empty
    assert(!route.empty());

    // return the first npu in route
    return route.front();
}

std::shared_ptr<Device> Chunk::next_device() const noexcept {
    // assert the chunk has next dest
    assert(!arrived_dest());

    // return next dest
    const auto next_dest = std::next(route.begin(), 1);
    return *next_dest;
}

void Chunk::mark_arrived_next_device() noexcept {
    // if this method is being called,
    // it means the chunk hasn't arrived its final dest yet
    assert(!arrived_dest());

    // pop previous node from the route
    // marking the current node has been changed
    route.pop_front();
}

bool Chunk::arrived_dest() const noexcept {
    // if a chunk arrived dest, route length should be 1
    // i.e., only containing the dest node
    return route.size() == 1;
}

ChunkSize Chunk::get_size() const noexcept {
    assert(chunk_size > 0);

    // return chunk size
    return chunk_size;
}

void Chunk::invoke_callback() noexcept {
    // invoke callback
    (*callback)(callback_arg);
}
