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

#ifndef __ALGORITHM_HH__
#define __ALGORITHM_HH__

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
#include "BaseStream.hh"
#include "CallData.hh"
#include "LogicalTopology.hh"

namespace AstraSim{
    class Algorithm:public Callable{
    public:
        enum class Name{Ring,DoubleBinaryTree,AllToAll};
        Name name;
        int id;
        BaseStream *stream;
        LogicalTopology *logicalTopology;
        int data_size;
        int final_data_size;
        ComType comType;
        bool enabled;
        int layer_num;
        Algorithm(int layer_num);
        virtual ~Algorithm()= default;
        virtual void run(EventType event,CallData *data)=0;
        virtual void exit();
        virtual void init(BaseStream *stream);
        virtual void call(EventType event,CallData *data);
    };
}
#endif