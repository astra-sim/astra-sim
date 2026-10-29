// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/ComputeNode.hh>

namespace AstraSim {

class Node : public ComputeNode {
  public:
    int id;
    Node* parent;
    Node* left_child;
    Node* right_child;
    Node(int id, Node* parent, Node* left_child, Node* right_child);
};

}  // namespace AstraSim
