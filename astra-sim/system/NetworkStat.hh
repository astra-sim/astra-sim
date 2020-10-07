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

#ifndef __NETWORKSTAT_HH__
#define __NETWORKSTAT_HH__

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
class NetworkStat{
public:
    std::list<double> net_message_latency;
    int net_message_counter;
    NetworkStat(){
        net_message_counter=0;
    }
    void update_network_stat(NetworkStat *networkStat){
        if(net_message_latency.size()<networkStat->net_message_latency.size()){
            int dif=networkStat->net_message_latency.size()-net_message_latency.size();
            for(int i=0;i<dif;i++){
                net_message_latency.push_back(0);
            }
        }
        std::list<double>::iterator it=net_message_latency.begin();
        for(auto &ml:networkStat->net_message_latency){
            (*it)+=ml;
            std::advance(it,1);
        }
        net_message_counter++;
    }
    void take_network_stat_average(){
        for(auto &ml:net_message_latency){
            ml/=net_message_counter;
        }
    }
};
}
#endif