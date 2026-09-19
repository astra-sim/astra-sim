#ifndef CHAKRA_FEEDER_V3_ET_FEEDER_H
#define CHAKRA_FEEDER_V3_ET_FEEDER_H

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include "cache.h"
#include "common.h"
#include "dependancy_solver.h"
#include "et_def.pb.h"
#include "et_feeder_node.h"

namespace std {
template <>
struct hash<
    std::tuple<Chakra::FeederV3::ETFeederId, Chakra::FeederV3::NodeId>> {
  size_t operator()(
      const std::tuple<Chakra::FeederV3::ETFeederId, Chakra::FeederV3::NodeId>&
          k) const {
    return std::hash<Chakra::FeederV3::ETFeederId>()(std::get<0>(k)) ^
        (std::hash<Chakra::FeederV3::NodeId>()(std::get<1>(k)) << 1);
  }
};
} // namespace std

namespace Chakra {
namespace FeederV3 {

class ETFeeder {
 public:
  ChakraGlobalMetadata global_metadata;
  ETFeeder(const std::string& file_path)
      : chakra_file(file_path, std::ios::binary | std::ios::in | std::ios::app),
        _feeder_id(_feeder_id_cnt++),
        dependancy_resolver(RESOLVE_DATA_DEPS, RESOLVE_CTRL_DEPS) {
    if (!chakra_file.is_open())
      throw std::runtime_error("Failed to open file " + file_path);
    this->build_index_dependancy_cache();
    this->graph_sanity_check(); // make sure graph is sane
  }

  ~ETFeeder() {
    // no explict cache release. Will be kicked-out natually.
    index_map.clear();
    chakra_file.close();
  }

  DependancyResolver& getDependancyResolver() {
    return dependancy_resolver;
  }

  std::shared_ptr<ETFeederNode> lookupNode(const NodeId& node_id);
  bool hasNodesToIssue();
  std::shared_ptr<ETFeederNode> getNextIssuableNode();
  void pushBackIssuableNode(const NodeId& node_id);
  void freeChildrenNodes(const NodeId& node_id);

  // legacy interface
  void addNode(std::shared_ptr<ETFeederNode> node);
  void removeNode(const NodeId& node_id);

  const uint64_t& feeder_id() const;

 private:
  std::ifstream chakra_file;

  static uint64_t _feeder_id_cnt;
  uint64_t _feeder_id;

  // shared global cache for storing chakra msgs.
  static Cache<std::tuple<ETFeederId, NodeId>, ChakraNode> _node_cache;

  std::unordered_map<NodeId, std::streampos> index_map;
  DependancyResolver dependancy_resolver;

  void build_index_dependancy_cache();
  std::shared_ptr<const ChakraNode> get_raw_chakra_node(NodeId node_id);
  friend class ETFeederNode;

  void graph_sanity_check();
};

} // namespace FeederV3
using ETFeeder = FeederV3::ETFeeder;
} // namespace Chakra

#endif
