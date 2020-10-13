/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __DOUBLEBINARYTREEALLREDUCE_HH__
#define __DOUBLEBINARYTREEALLREDUCE_HH__

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
#include "astra-sim/system/CallData.hh"
#include "astra-sim/system/topology/BinaryTree.hh"
#include "Algorithm.hh"

namespace AstraSim{
    class DoubleBinaryTreeAllReduce:public Algorithm{
    public:
        enum class State{Begin,WaitingForTwoChildData,WaitingForOneChildData,SendingDataToParent,WaitingDataFromParent,SendingDataToChilds,End};
        void run(EventType event,CallData *data);
        //void call(EventType event,CallData *data);
        //void exit();
        int parent;
        int left_child;
        int reductions;
        int right_child;
        //BinaryTree *tree;
        BinaryTree::Type type;
        State state;
        DoubleBinaryTreeAllReduce(int id,int layer_num,BinaryTree *tree,int data_size,bool boost_mode);
        //void init(BaseStream *stream);
    };
}
#endif