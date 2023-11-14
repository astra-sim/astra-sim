/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/BaseStream.hh"

#include <memory>
#include <sstream>

#include "astra-sim/system/StreamBaseline.hh"
#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using namespace AstraSim;

static auto logger = spdlog::stdout_color_mt("system::BaseStream");
static std::unique_ptr<std::stringstream> sstream_buffer =
    std::make_unique<std::stringstream>();

std::map<int, int> BaseStream::synchronizer;
std::map<int, int> BaseStream::ready_counter;
std::map<int, std::list<BaseStream*>> BaseStream::suspended_streams;

void BaseStream::changeState(StreamState state) {
  this->state = state;
}

BaseStream::BaseStream(
    int stream_id,
    Sys* owner,
    std::list<CollectivePhase> phases_to_go) {
  this->stream_id = stream_id;
  this->owner = owner;
  this->initialized = false;
  this->phases_to_go = phases_to_go;
  if (synchronizer.find(stream_id) != synchronizer.end()) {
    synchronizer[stream_id]++;
  } else {
    logger->debug("synchronizer set!");
    synchronizer[stream_id] = 1;
    ready_counter[stream_id] = 0;
  }
  for (auto& vn : phases_to_go) {
    if (vn.algorithm != nullptr) {
      vn.init(this);
    }
  }
  state = StreamState::Created;
  preferred_scheduling = SchedulingPolicy::None;
  creation_time = Sys::boostedTick();
  total_packets_sent = 0;
  current_queue_id = -1;
  priority = 0;
}
