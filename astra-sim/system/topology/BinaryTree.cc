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

#include "BinaryTree.hh"
namespace AstraSim {
BinaryTree::~BinaryTree() {
  for (auto n : node_list) {
    delete n.second;
  }
}
BinaryTree::BinaryTree(int id, TreeType tree_type, int total_tree_nodes,
                       int start, int stride)
    : BasicLogicalTopology(BasicLogicalTopology::BasicTopology::BinaryTree) {
  this->total_tree_nodes = total_tree_nodes;
  this->start = start;
  this->tree_type = tree_type;
  this->stride = stride;
  tree = new Node(-1, NULL, NULL, NULL);
  int depth = 1;
  int tmp = total_tree_nodes;
  // node_list.resize(total_nodes,NULL);
  while (tmp > 1) {
    depth++;
    tmp /= 2;
  }
  if (tree_type == TreeType::RootMin) {
    tree->right_child = initialize_tree(depth - 1, tree);
  } else {
    tree->left_child = initialize_tree(depth - 1, tree);
  }
  build_tree(tree);
  // std::cout<<"##############################################"<<std::endl;
  // print(tree);
  // std::cout<<"##############################################"<<std::endl;
}
Node *BinaryTree::initialize_tree(int depth, Node *parent) {
  Node *tmp = new Node(-1, parent, NULL, NULL);
  if (depth > 1) {
    tmp->left_child = initialize_tree(depth - 1, tmp);
    tmp->right_child = initialize_tree(depth - 1, tmp);
  }
  return tmp;
}
void BinaryTree::build_tree(Node *node) {
  if (node->left_child != NULL) {
    build_tree(node->left_child);
  }
  node->id = start;
  node_list[start] = node;
  start += stride;
  if (node->right_child != NULL) {
    build_tree(node->right_child);
  }
  return;
}
int BinaryTree::get_parent_id(int id) {
  Node *parent = this->node_list[id]->parent;
  if (parent != NULL) {
    return parent->id;
  }
  return -1;
}
int BinaryTree::get_right_child_id(int id) {
  Node *child = this->node_list[id]->right_child;
  if (child != NULL) {
    return child->id;
  }
  return -1;
}
int BinaryTree::get_left_child_id(int id) {
  Node *child = this->node_list[id]->left_child;
  if (child != NULL) {
    return child->id;
  }
  return -1;
}
BinaryTree::Type BinaryTree::get_node_type(int id) {
  Node *node = this->node_list[id];
  if (node->parent == NULL) {
    return Type::Root;
  } else if (node->left_child == NULL && node->right_child == NULL) {
    return Type::Leaf;
  } else {
    return Type::Intermediate;
  }
}
void BinaryTree::print(Node *node) {
  std::cout << "I am node: " << node->id;
  if (node->left_child != NULL) {
    std::cout << " and my left child is: " << node->left_child->id;
  }
  if (node->right_child != NULL) {
    std::cout << " and my right child is: " << node->right_child->id;
  }
  if (node->parent != NULL) {
    std::cout << " and my parent is: " << node->parent->id;
  }
  BinaryTree::Type typ = get_node_type(node->id);
  if (typ == BinaryTree::Type::Root) {
    std::cout << " and I am Root ";
  } else if (typ == BinaryTree::Type::Intermediate) {
    std::cout << " and I am Intermediate ";
  } else if (typ == BinaryTree::Type::Leaf) {
    std::cout << " and I am Leaf ";
  }
  std::cout << std::endl;
  if (node->left_child != NULL) {
    print(node->left_child);
  }
  if (node->right_child != NULL) {
    print(node->right_child);
  }
}
} // namespace AstraSim