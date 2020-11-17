/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "BaseStream.hh"
#include "StreamBaseline.hh"
namespace AstraSim {
std::map<int, int> BaseStream::synchronizer;
std::map<int, int> BaseStream::ready_counter;
std::map<int, std::list<BaseStream*>> BaseStream::suspended_streams;
void BaseStream::changeState(StreamState state) {
  this->state = state;
}
BaseStream::BaseStream(
    int stream_num,
    Sys* owner,
    std::list<CollectivePhase> phases_to_go) {
  this->stream_num = stream_num;
  this->owner = owner;
  this->initialized = false;
  this->phases_to_go = phases_to_go;
  if (synchronizer.find(stream_num) != synchronizer.end()) {
    synchronizer[stream_num]++;
  } else {
    // std::cout<<"synchronizer set!"<<std::endl;
    synchronizer[stream_num] = 1;
    ready_counter[stream_num] = 0;
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
void BaseStream::declare_ready() {
  ready_counter[stream_num]++;
  if (ready_counter[stream_num] == owner->total_nodes) {
    synchronizer[stream_num] = owner->total_nodes;
    ready_counter[stream_num] = 0;
  }
}
bool BaseStream::is_ready() {
  return synchronizer[stream_num] > 0;
}
void BaseStream::consume_ready() {
  // std::cout<<"consume ready called!"<<std::endl;
  assert(synchronizer[stream_num] > 0);
  synchronizer[stream_num]--;
  resume_ready(stream_num);
}
void BaseStream::suspend_ready() {
  if (suspended_streams.find(stream_num) == suspended_streams.end()) {
    std::list<BaseStream*> empty;
    suspended_streams[stream_num] = empty;
  }
  suspended_streams[stream_num].push_back(this);
  return;
}
void BaseStream::resume_ready(int st_num) {
  if (suspended_streams[st_num].size() != (owner->all_generators.size() - 1)) {
    return;
  }
  int counter = owner->all_generators.size() - 1;
  for (int i = 0; i < counter; i++) {
    StreamBaseline* stream = (StreamBaseline*)suspended_streams[st_num].front();
    suspended_streams[st_num].pop_front();
    owner->all_generators[stream->owner->id]->proceed_to_next_vnet_baseline(
        stream);
  }
  if (phases_to_go.size() == 0) {
    destruct_ready();
  }
  return;
}
void BaseStream::destruct_ready() {
  std::map<int, int>::iterator it;
  it = synchronizer.find(stream_num);
  synchronizer.erase(it);
  ready_counter.erase(stream_num);
  suspended_streams.erase(stream_num);
}
} // namespace AstraSim