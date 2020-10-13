/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __RECVPACKETEVENTHADNDLERDATA_HH__
#define __RECVPACKETEVENTHADNDLERDATA_HH__

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
#include "BasicEventHandlerData.hh"
#include "BaseStream.hh"

namespace AstraSim{
    class RecvPacketEventHadndlerData:public BasicEventHandlerData,public MetaData{
    public:
        BaseStream *owner;
        int vnet;
        int stream_num;
        bool message_end;
        Tick ready_time;
        RecvPacketEventHadndlerData(BaseStream *owner,int nodeId, EventType event,int vnet,int stream_num);
    };
}
#endif