// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <map>

#include <astra-sim/common/Common.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/BasicLogicalTopology.hh>
#include <astra-sim/system/simple/astraccl/native_collectives/logical_topology/Node.hh>

namespace AstraSim {

class BinaryTree : public BasicLogicalTopology {
  public:
    enum class TreeType { RootMax, RootMin };
    enum class Type { Leaf, Root, Intermediate };

    BinaryTree(int id,
               TreeType tree_type,
               int total_tree_nodes,
               int start,
               int stride);
    virtual ~BinaryTree();

    int get_num_of_nodes_in_dimension(int dimension) override {
        return total_tree_nodes;
    }

    Node* initialize_tree(int depth, Node* parent);
    void build_tree(Node* node);
    int get_parent_id(int id);
    int get_left_child_id(int id);
    int get_right_child_id(int id);
    Type get_node_type(int id);
    void print(Node* node);

    int total_tree_nodes;
    int start;
    TreeType tree_type;
    int stride;
    Node* tree;
    std::map<int, Node*> node_list;
};

}  // namespace AstraSim
