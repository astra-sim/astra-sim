#include "et_feeder_node.h"
#include "et_feeder.h"

using namespace Chakra::FeederV3;

std::shared_ptr<const ChakraNode> ETFeederNode::get_chakra_node() const {
  if (this->chakra_node.expired()) {
    auto node = this->feeder.get_raw_chakra_node(this->node_id);
    this->chakra_node = node;
    return node;
  }
  return this->chakra_node.lock();
}

bool ETFeederNode::has_attr(const std::string& attr_name) const {
  const auto node = this->get_chakra_node();
  for (auto& attr : node->attr())
    if (attr.name() == attr_name)
      return true;
  return false;
}

const ChakraAttr ETFeederNode::get_attr_msg(
    const std::string& attr_name) const {
  const auto node = this->get_chakra_node();
  for (auto& attr : node->attr())
    if (attr.name() == attr_name)
      return attr;
  this->complain_attr_not_found(attr_name);
}

bool ETFeederNode::get_attr_msg(
    const std::string& attr_name,
    const ChakraAttr** attr) const {
  const auto node = this->get_chakra_node();
  for (auto& iter_attr : node->attr())
    if (iter_attr.name() == attr_name) {
      *attr = &iter_attr;
      return true;
    }
  return false;
}

ChakraAttr::ValueCase ETFeederNode::get_attr_type(
    const ChakraAttr& attr) const {
  return attr.value_case();
}

NodeId ETFeederNode::id() const {
  auto node = this->get_chakra_node();
  return node->id();
}

std::string ETFeederNode::name() const {
  auto node = this->get_chakra_node();
  return node->name();
}

ChakraProtoMsg::NodeType ETFeederNode::type() const {
  auto node = this->get_chakra_node();
  return node->type();
}

uint64_t ETFeederNode::runtime() const {
  auto node = this->get_chakra_node();
  return node->duration_micros();
}

bool ETFeederNode::is_cpu_op() const {
  return this->is_cpu_op<bool>();
}

uint64_t ETFeederNode::num_ops() const {
  return this->num_ops<uint64_t>();
}

uint32_t ETFeederNode::tensor_loc() const {
  return this->tensor_loc<uint32_t>();
}

uint64_t ETFeederNode::tensor_size() const {
  return this->tensor_size<uint64_t>();
}

ChakraProtoMsg::CollectiveCommType ETFeederNode::comm_type() const {
  return static_cast<ChakraProtoMsg::CollectiveCommType>(
      this->comm_type<uint64_t>());
}

uint32_t ETFeederNode::comm_priority() const {
  return this->comm_priority<uint32_t>();
}

uint64_t ETFeederNode::comm_size() const {
  return this->comm_size<uint64_t>();
}

uint32_t ETFeederNode::comm_src() const {
  return this->comm_src<uint32_t>();
}

uint32_t ETFeederNode::comm_dst() const {
  return this->comm_dst<uint32_t>();
}

uint32_t ETFeederNode::comm_tag() const {
  return this->comm_tag<uint32_t>();
}

std::string ETFeederNode::get_inputs_values(const std::string& default_) const {
  auto node = this->get_chakra_node();
  if (node->has_inputs()) {
    return node->inputs().values();
  }
  return default_;
}

std::string ETFeederNode::get_inputs_shapes(const std::string& default_) const {
  auto node = this->get_chakra_node();
  if (node->has_inputs()) {
    return node->inputs().shapes();
  }
  return default_;
}

std::string ETFeederNode::get_inputs_types(const std::string& default_) const {
  auto node = this->get_chakra_node();
  if (node->has_inputs()) {
    return node->inputs().types();
  }
  return default_;
}

std::string ETFeederNode::get_outputs_values(
    const std::string& default_) const {
  auto node = this->get_chakra_node();
  if (node->has_outputs()) {
    return node->outputs().values();
  }
  return default_;
}

std::string ETFeederNode::get_outputs_shapes(
    const std::string& default_) const {
  auto node = this->get_chakra_node();
  if (node->has_outputs()) {
    return node->outputs().shapes();
  }
  return default_;
}

std::string ETFeederNode::get_outputs_types(const std::string& default_) const {
  auto node = this->get_chakra_node();
  if (node->has_outputs()) {
    return node->outputs().types();
  }
  return default_;
}

[[noreturn]] void ETFeederNode::complain_attr_not_found(
    const std::string& attr_name) const {
  throw std::runtime_error(
      "Attribute " + attr_name + " not found in node " +
      std::to_string(this->node_id) +
      " feeder->id=" + std::to_string(this->feeder.feeder_id()));
}
