// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/system/common/CallData.hh>
#include <astra-sim/system/simple/astraccl/Algorithm.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/BinaryTree.hh>

namespace AstraSim {
struct WFReq;
class DoubleBinaryTreeAllReduce : public Algorithm {
  public:
    enum class State {
        Begin = 0,
        WaitingForTwoChildData,
        WaitingForOneChildData,
        SendingDataToParent,
        WaitingDataFromParent,
        SendingDataToChilds,
        End
    };

    DoubleBinaryTreeAllReduce(int id, BinaryTree* tree, uint64_t data_size);
    void run(EventType event, CallData* data);

    BinaryTree::Type type;
    State state;
    int parent;
    int left_child;
    int reductions;
    int right_child;
};

}  // namespace AstraSim
