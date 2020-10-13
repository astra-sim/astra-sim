/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __STREAMBASELINE_HH__
#define __STREAMBASELINE_HH__

#include <map>
#include <math.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <tuple>
#include <cstdint>
#include <list>
#include <vector>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <assert.h>
#include "Common.hh"
#include "BaseStream.hh"
#include "CollectivePhase.hh"
#include "RecvPacketEventHadndlerData.hh"

namespace AstraSim{
    class Sys;
    class StreamBaseline: public BaseStream
    {
    public:
        StreamBaseline(Sys *owner,DataSet *dataset,int stream_num,std::list<CollectivePhase> phases_to_go,int priority);
        void init();
        void call(EventType event,CallData *data);
        void consume(RecvPacketEventHadndlerData *message);
        //~StreamBaseline()= default;
    };
}
#endif