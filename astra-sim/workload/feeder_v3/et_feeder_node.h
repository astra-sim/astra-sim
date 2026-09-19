#ifndef CHAKRA_FEEDER_V3_ET_FEEDER_NODE_H
#define CHAKRA_FEEDER_V3_ET_FEEDER_NODE_H

#include <functional>
#include <memory>
#include "common.h"
#include "et_def.pb.h"

namespace Chakra {
namespace FeederV3 {
class ETFeeder;

class ETFeederNode {
 public:
  ETFeederNode(ETFeeder& etfeeder, NodeId node_id)
      : feeder(etfeeder), node_id(node_id) {}

  bool has_attr(const std::string& attr_name) const;

  const ChakraAttr get_attr_msg(const std::string& attr_name) const;
  bool get_attr_msg(const std::string& attr_name, const ChakraAttr** attr)
      const;

  ChakraAttr::ValueCase get_attr_type(const ChakraAttr& attr) const;

  template <typename T>
  T get_attr(
      const ChakraAttr& attr,
      const bool strict_type = DEFAULT_STRICT_TYPING) const;

  template <typename T>
  T get_attr(const std::string& attr_name, const T& default_value) const;

  template <typename T>
  T get_attr(const std::string& attr_name) const;

#define REGISTER_ATTR_WITH_DEFAULT(attr_name, system_default_value) \
  template <typename T>                                             \
  T attr_name() const {                                             \
    return this->get_attr<T>(#attr_name, (system_default_value));   \
  }                                                                 \
  template <typename T>                                             \
  T attr_name(const T& default_value) const {                       \
    return this->get_attr<T>(#attr_name, default_value);            \
  }

#define REGISTER_ATTR(attr_name)                         \
  template <typename T>                                  \
  T attr_name() const {                                  \
    return this->get_attr<T>(#attr_name);                \
  }                                                      \
  template <typename T>                                  \
  T attr_name(const T& default_value) const {            \
    return this->get_attr<T>(#attr_name, default_value); \
  }

// please mod the following header to add any new attributes
#include "et_feeder_node_attr.h"

  NodeId id() const;
  std::string name() const;
  ChakraProtoMsg::NodeType type() const;
  uint64_t runtime() const;

  // old interface
  bool is_cpu_op() const;
  uint64_t num_ops() const;
  uint32_t tensor_loc() const;
  uint64_t tensor_size() const;
  ChakraProtoMsg::CollectiveCommType comm_type() const;
  uint32_t comm_priority() const;
  uint64_t comm_size() const;
  uint32_t comm_src() const;
  uint32_t comm_dst() const;
  uint32_t comm_tag() const;

  std::string get_inputs_values(const std::string& default_ = "") const;
  std::string get_inputs_shapes(const std::string& default_ = "") const;
  std::string get_inputs_types(const std::string& default_ = "") const;
  std::string get_outputs_values(const std::string& default_ = "") const;
  std::string get_outputs_shapes(const std::string& default_ = "") const;
  std::string get_outputs_types(const std::string& default_ = "") const;

 private:
  template <typename T>
  class _TypeConverter {
   public:
    template <typename F>
    static std::enable_if_t<std::is_same_v<F, T>, T> strict_converter(F value) {
      return value;
    }

    template <typename F>
    static std::enable_if_t<!std::is_same_v<F, T>, T> strict_converter(
        F value) {
      throw std::bad_cast();
    }

    template <typename F>
    static T flagged_implicit_converter(F value) {
      if constexpr (std::is_integral_v<T>) {
        // integer to integer
        if constexpr (
            ALLOW_IMPLICIT_INTEGER_CONVERSION && std::is_integral_v<F>)
          return static_cast<T>(value);
        // float to integer
        if constexpr (
            ALLOW_IMPLICIT_FLOAT_TO_INTEGER_CONVERSION &&
            std::is_floating_point_v<F>)
          return static_cast<T>(value);
      } else if constexpr (std::is_floating_point_v<T>) {
        // float to float
        if constexpr (
            ALLOW_IMPLICIT_FLOAT_CONVERSION && std::is_floating_point_v<F>)
          return static_cast<T>(value);
        // integer to float
        if constexpr (
            ALLOW_IMPLICIT_INTEGER_TO_FLOAT_CONVERSION && std::is_integral_v<F>)
          return static_cast<T>(value);
      }
      return strict_converter(value);
    }

    template <typename F>
    static std::enable_if_t<std::is_convertible_v<F, T>, T> implicit_converter(
        F value) {
      return static_cast<T>(value);
    }

    template <typename F>
    static std::enable_if_t<!std::is_convertible_v<F, T>, T> implicit_converter(
        F value) {
      throw std::bad_cast();
    }
  };
  // have to use this to help later template function access feeder field,
  // without knowing whole def of etfeeder
  [[noreturn]] void complain_attr_not_found(const std::string& attr_name) const;

  // ETFeederNode only store minimal thing to reduce memory usage.
  ETFeeder& feeder;
  NodeId node_id;
  mutable std::weak_ptr<const ChakraNode> chakra_node;

  std::shared_ptr<const ChakraNode> get_chakra_node() const;
};

template <typename T>
T ETFeederNode::get_attr(const ChakraAttr& attr, const bool strict_type) const {
  {
    // change to implicit if user prefer here.
    auto cvt = [&](auto value) -> T {
      if (strict_type) {
        return _TypeConverter<T>::strict_converter(value);
      } else {
        return _TypeConverter<T>::flagged_implicit_converter(value);
      }
    };
    switch (attr.value_case()) {
      case ChakraAttr::kDoubleVal:
        return cvt(attr.double_val());
      case ChakraAttr::kFloatVal:
        return cvt(attr.float_val());
      case ChakraAttr::kInt32Val:
        return cvt(attr.int32_val());
      case ChakraAttr::kInt64Val:
        return cvt(attr.int64_val());
      case ChakraAttr::kUint32Val:
        return cvt(attr.uint32_val());
      case ChakraAttr::kUint64Val:
        return cvt(attr.uint64_val());
      case ChakraAttr::kSint32Val:
        return cvt(attr.sint32_val());
      case ChakraAttr::kSint64Val:
        return cvt(attr.sint64_val());
      case ChakraAttr::kFixed32Val:
        return cvt(attr.fixed32_val());
      case ChakraAttr::kFixed64Val:
        return cvt(attr.fixed64_val());
      case ChakraAttr::kSfixed32Val:
        return cvt(attr.sfixed32_val());
      case ChakraAttr::kSfixed64Val:
        return cvt(attr.sfixed64_val());
      case ChakraAttr::kBoolVal:
        return cvt(attr.bool_val());
      case ChakraAttr::kStringVal:
        return cvt(attr.string_val());
      case ChakraAttr::kBytesVal:
        return cvt(attr.bytes_val());
      default:
        // TODO: support list types
        // TODO: maybe let use register their own converter for complicate
        // types?
        throw std::invalid_argument(
            "Attribute type not supported, for list types please handle them manually");
    }
  }
}

template <typename T>
T ETFeederNode::get_attr(const std::string& attr_name, const T& default_value)
    const {
  // option 1 or 3: user provide default value or systemwise default value
  if (this->has_attr(attr_name)) {
    const auto attr = this->get_attr_msg(attr_name);
    return this->get_attr<T>(attr, DEFAULT_STRICT_TYPING);
  }
  return default_value;
}

template <typename T>
T ETFeederNode::get_attr(const std::string& attr_name) const {
  // option 2: throw complaints
  if (this->has_attr(attr_name)) {
    const auto attr = this->get_attr_msg(attr_name);
    return this->get_attr<T>(attr, DEFAULT_STRICT_TYPING);
  }
  this->complain_attr_not_found(attr_name);
}
template <>
inline ChakraAttr ETFeederNode::get_attr<ChakraAttr>(
    const ChakraAttr& attr,
    const bool /*strict_type*/) const {
  return attr;
}

} // namespace FeederV3
using ETFeederNode = FeederV3::ETFeederNode;
} // namespace Chakra

#endif
