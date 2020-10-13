/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __DATASET_HH__
#define __DATASET_HH__

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
#include "CallData.hh"
#include "StreamStat.hh"

namespace AstraSim{
    class DataSet:public Callable,public StreamStat{
    public:
        static int id_auto_increment;
        int my_id;
        int total_streams;
        int finished_streams;
        bool finished;
        Tick finish_tick;
        Tick creation_tick;
        std::pair<Callable *,EventType> *notifier;

        DataSet(int total_streams);
        void set_notifier(Callable *layer,EventType event);
        void notify_stream_finished(StreamStat *data);
        void call(EventType event,CallData *data);
        bool is_finished();
        //~DataSet()= default;
    };
}
#endif