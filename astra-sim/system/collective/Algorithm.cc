/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/collective/Algorithm.hh"

using namespace AstraSim;

Algorithm::Algorithm() {
  enabled = true;
}

void Algorithm::init(BaseStream* stream) {
  this->stream = stream;
}

void Algorithm::call(EventType event, CallData* data) {
}

void Algorithm::exit() {
  stream->owner->proceed_to_next_vnet_baseline((StreamBaseline*)stream);
}
