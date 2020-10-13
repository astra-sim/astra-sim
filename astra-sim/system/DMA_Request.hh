/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __DMAREQUEST_HH__
#define __DMAREQUEST_HH__

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
#include "Callable.hh"

namespace AstraSim{
    class DMA_Request{
    public:
        int id;
        int slots;
        int latency;
        bool executed;
        int bytes;
        Callable *stream_owner;
        DMA_Request(int id,int slots,int latency,int bytes);
        DMA_Request(int id,int slots,int latency,int bytes,Callable *stream_owner);
    };
}
#endif