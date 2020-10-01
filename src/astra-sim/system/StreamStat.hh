/******************************************************************************
Copyright (c) 2020 Georgia Institute of Technology
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Author : Saeed Rashidi (saeed.rashidi@gatech.edu)
*******************************************************************************/

#ifndef __STREAMSTAT_HH__
#define __STREAMSTAT_HH__

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
#include "SharedBusStat.hh"
#include "NetworkStat.hh"

namespace AstraSim{
class StreamStat:public SharedBusStat,public NetworkStat{
public:
    std::list<double > queuing_delay;
    int stream_stat_counter;
    //~StreamStat()= default;
    StreamStat():SharedBusStat(BusType::Shared,0,0,0,0){
        stream_stat_counter=0;
    }
    void update_stream_stats(StreamStat *streamStat){
        update_bus_stats(BusType::Both,streamStat);
        update_network_stat(streamStat);
        if(queuing_delay.size()<streamStat->queuing_delay.size()){
            int dif=streamStat->queuing_delay.size()-queuing_delay.size();
            for(int i=0;i<dif;i++){
                queuing_delay.push_back(0);
            }
        }
        std::list<double>::iterator it=queuing_delay.begin();
        for(auto &tick:streamStat->queuing_delay){
            (*it)+=tick;
            std::advance(it,1);
        }
        stream_stat_counter++;
    }
    void take_stream_stats_average(){
        take_bus_stats_average();
        take_network_stat_average();
        for(auto &tick:queuing_delay){
            tick/=stream_stat_counter;
        }
    }
};
}
#endif