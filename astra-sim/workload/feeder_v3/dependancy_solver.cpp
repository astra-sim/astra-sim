#include "dependancy_solver.h"
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

using namespace Chakra::FeederV3;

void _DependancyLayer::add_node(
    const NodeId& node,
    const std::unordered_set<NodeId>& parents) {
  std::unique_lock<std::shared_mutex> lock(this->mutex);
  this->dirty = true;
  this->_helper_allocate_bucket(node);
  for (auto& parent : parents) {
    this->_helper_allocate_bucket(parent);
    this->child_map_parent[node].insert(parent);
    this->parent_map_child[parent].insert(node);
  }
}

void _DependancyLayer::add_node_children(
    const NodeId& node,
    const std::unordered_set<NodeId>& children) {
  std::unique_lock<std::shared_mutex> lock(this->mutex);
  this->dirty = true;
  this->_helper_allocate_bucket(node);
  for (auto& child : children) {
    this->_helper_allocate_bucket(child);
    this->child_map_parent[child].insert(node);
    this->parent_map_child[node].insert(child);
  }
}

void _DependancyLayer::take_node(const NodeId& node) {
  std::unique_lock<std::shared_mutex> lock(this->mutex);
  if (this->dirty) {
    throw std::runtime_error(
        "dependancy layer is dirty, resolve_dependancy_free_nodes should be called first");
  }
  if (this->dependancy_free_nodes.find(node) ==
      this->dependancy_free_nodes.end()) {
    const auto& parents = this->child_map_parent[node];
    throw std::runtime_error(
        "Node " + std::to_string(node) +
        " is not dependancy free or already taken/released");
  }
  if (this->ongoing_nodes.find(node) != this->ongoing_nodes.end()) {
    throw std::runtime_error("Node is already taken");
  }
  this->ongoing_nodes.insert(node);
  this->dependancy_free_nodes.erase(node);
}

void _DependancyLayer::finish_node(const NodeId& node) {
  std::unique_lock<std::shared_mutex> lock(this->mutex);
  if (this->dirty) {
    throw std::runtime_error(
        "dependancy layer is dirty, resolve_dependancy_free_nodes should be called first");
  }
  if (this->ongoing_nodes.find(node) == this->ongoing_nodes.end()) {
    throw std::runtime_error("Node is not taken");
  }
  this->ongoing_nodes.erase(node);
  for (auto& child : this->parent_map_child[node]) {
    if (this->child_map_parent[child].find(node) ==
        this->child_map_parent[child].end()) {
      // This should not happen, but sanity check
      throw std::runtime_error(
          "Parent map child is not consistent with child map parent");
    }
    this->child_map_parent[child].erase(node);
    if (this->child_map_parent[child].empty()) {
      this->dependancy_free_nodes.insert(child);
    }
  }
  this->child_map_parent.erase(node);
  this->parent_map_child.erase(node);
}

void _DependancyLayer::push_back_node(const NodeId& node) {
  std::unique_lock<std::shared_mutex> lock(this->mutex);
  if (this->dirty) {
    throw std::runtime_error(
        "dependancy layer is dirty, resolve_dependancy_free_nodes should be called first");
  }
  if (this->ongoing_nodes.find(node) == this->ongoing_nodes.end()) {
    throw std::runtime_error("Node is not taken");
  }
  this->ongoing_nodes.erase(node);
  this->dependancy_free_nodes.insert(node);
}

void _DependancyLayer::resolve_dependancy_free_nodes() {
  std::unique_lock<std::shared_mutex> lock(this->mutex);
  if ((!this->dependancy_free_nodes.empty()) || (!this->ongoing_nodes.empty()))
    throw std::runtime_error(
        "resolve_dependancy_free_nodes after initialization is not supported yet!");
  for (auto& it : this->child_map_parent) {
    auto& node = it.first;
    auto& parents = it.second;
    if (parents.empty())
      this->dependancy_free_nodes.insert(node);
  }
  if (this->dependancy_free_nodes.empty())
    throw std::runtime_error(
        "No dependancy free nodes found, there might be deadlocks");
  this->dirty = false;
}

const std::unordered_set<NodeId>& _DependancyLayer::get_dependancy_free_nodes()
    const {
  return this->dependancy_free_nodes;
}

const std::unordered_set<NodeId>& _DependancyLayer::get_children(
    NodeId node) const {
  const auto& results = this->parent_map_child.at(node);
  return this->parent_map_child.at(node);
}

const std::unordered_set<NodeId>& _DependancyLayer::get_parents(
    NodeId node) const {
  return this->child_map_parent.at(node);
}

const std::unordered_set<NodeId>& _DependancyLayer::get_ongoing_nodes() const {
  return this->ongoing_nodes;
}

void _DependancyLayer::_helper_allocate_bucket(NodeId node_id) {
  if (this->child_map_parent.find(node_id) == this->child_map_parent.end()) {
    this->child_map_parent[node_id] = std::unordered_set<NodeId>();
  }
  if (this->parent_map_child.find(node_id) == this->parent_map_child.end()) {
    this->parent_map_child[node_id] = std::unordered_set<NodeId>();
  }
}

void DependancyResolver::add_node(const ChakraNode& node) {
  NodeId node_id = node.id();
  std::unordered_set<NodeId> parents, enabled_parents;
  for (auto& parent : node.data_deps()) {
    if (this->enable_data_deps)
      enabled_parents.insert(parent);
    parents.insert(parent);
  }
  this->data_dependancy.add_node(node_id, parents);
  parents.clear();

  for (auto& parent : node.ctrl_deps()) {
    if (this->enable_ctrl_deps)
      enabled_parents.insert(parent);
    parents.insert(parent);
  }
  this->ctrl_dependancy.add_node(node_id, parents);
  parents.clear();

  this->enabled_dependancy.add_node(node_id, enabled_parents);
}

void DependancyResolver::take_node(const NodeId& node) {
  this->data_dependancy.take_node(node);
  this->ctrl_dependancy.take_node(node);
  this->enabled_dependancy.take_node(node);
}

void DependancyResolver::push_back_node(const NodeId& node) {
  this->data_dependancy.push_back_node(node);
  this->ctrl_dependancy.push_back_node(node);
  this->enabled_dependancy.push_back_node(node);
}

void DependancyResolver::finish_node(const NodeId& node) {
  this->data_dependancy.finish_node(node);
  this->ctrl_dependancy.finish_node(node);
  this->enabled_dependancy.finish_node(node);
}

void DependancyResolver::resolve_dependancy_free_nodes() {
  this->data_dependancy.resolve_dependancy_free_nodes();
  this->ctrl_dependancy.resolve_dependancy_free_nodes();
  this->enabled_dependancy.resolve_dependancy_free_nodes();
}

const std::unordered_set<NodeId>& DependancyResolver::
    get_dependancy_free_nodes() const {
  return this->enabled_dependancy.get_dependancy_free_nodes();
}

const std::unordered_set<NodeId>& DependancyResolver::get_ongoing_nodes()
    const {
  return this->enabled_dependancy.get_ongoing_nodes();
}

const _DependancyLayer& DependancyResolver::get_data_dependancy() const {
  return this->data_dependancy;
}

const _DependancyLayer& DependancyResolver::get_ctrl_dependancy() const {
  return this->ctrl_dependancy;
}

const _DependancyLayer& DependancyResolver::get_enabled_dependancy() const {
  return this->enabled_dependancy;
}

_DependancyLayer& DependancyResolver::get_data_dependancy_mut() {
  return this->data_dependancy;
}

_DependancyLayer& DependancyResolver::get_ctrl_dependancy_mut() {
  return this->ctrl_dependancy;
}

_DependancyLayer& DependancyResolver::get_enabled_dependancy_mut() {
  return this->enabled_dependancy;
}
