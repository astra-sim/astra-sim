// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/Node.hh>

using namespace AstraSim;

Node::Node(int id, Node* parent, Node* left_child, Node* right_child) {
    this->id = id;
    this->parent = parent;
    this->left_child = left_child;
    this->right_child = right_child;
}
