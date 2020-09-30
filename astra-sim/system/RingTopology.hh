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

#ifndef __RINGTOPOLOGY_HH__
#define __RINGTOPOLOGY_HH__

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
#include "BasicLogicalTopology.hh"

namespace AstraSim{
    class RingTopology:public BasicLogicalTopology{
    public:
        enum class Direction{Clockwise,Anticlockwise};
        enum class Dimension{Local,Vertical,Horizontal};
        std::string name;
        int id;
        int next_node_id;
        int previous_node_id;
        int offset;
        int total_nodes_in_ring;
        int index_in_ring;
        Dimension dimension;
        RingTopology(Dimension dimension,int id,int total_nodes_in_ring,int index_in_ring,int offset);
        void find_neighbors();
        virtual int get_receiver_node(int node_id,Direction direction);
        virtual int get_sender_node(int node_id,Direction direction);
        int get_nodes_in_ring();
        bool is_enabled();
    private:
        std::map<int,int> id_to_index;
    };
}
#endif