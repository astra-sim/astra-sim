/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
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
#include "astra-sim/system/Common.hh"
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