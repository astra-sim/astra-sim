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

#ifndef __MEMBUS_HH__
#define __MEMBUS_HH__

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
    class Sys;
    class LogGP;
    class MemBus{
    public:
        enum class Transmition{Fast,Usual};
        LogGP *NPU_side;
        LogGP *MA_side;
        Sys *generator;
        int communication_delay;
        bool model_shared_bus;
        ~MemBus();
        MemBus(std::string side1,std::string side2,Sys *generator,Tick L,Tick o,Tick g,double G,bool model_shared_bus,int communication_delay,bool attach);
        void send_from_NPU_to_MA(Transmition transmition,int bytes,bool processed,bool send_back,Callable *callable);
        void send_from_MA_to_NPU(Transmition transmition,int bytes,bool processed,bool send_back,Callable *callable);
    };
}
#endif