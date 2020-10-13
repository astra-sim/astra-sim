/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __COLLECTIVEPHASE_HH__
#define __COLLECTIVEPHASE_HH__

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

namespace AstraSim{
    class Sys;
    class Algorithm;
    class BaseStream;
    class CollectivePhase{
    public:
        Sys *generator;
        int queue_id;
        int initial_data_size;
        int final_data_size;
        bool enabled;
        ComType comm_type;
        Algorithm *algorithm;
        CollectivePhase(Sys *generator,int queue_id,Algorithm *algorithm);
        CollectivePhase();
        void init(BaseStream *stream);
    };
}
#endif