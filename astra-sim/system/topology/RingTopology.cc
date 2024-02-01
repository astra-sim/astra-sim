/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/topology/RingTopology.hh"

#include <cassert>
#include <iostream>

#include "astra-sim/common/Logging.hh"

using namespace std;
using namespace AstraSim;

RingTopology::RingTopology(Dimension dimension, int id, std::vector<int> NPUs)
    : BasicLogicalTopology(BasicLogicalTopology::BasicTopology::Ring) {
  name = "local";
  if (dimension == Dimension::Vertical) {
    name = "vertical";
  } else if (dimension == Dimension::Horizontal) {
    name = "horizontal";
  }
  this->id = id;
  this->total_nodes_in_ring = NPUs.size();
  this->dimension = dimension;
  this->offset = -1;
  this->index_in_ring = -1;
  for (int i = 0; i < total_nodes_in_ring; i++) {
    id_to_index[NPUs[i]] = i;
    index_to_id[i] = NPUs[i];
    if (id == NPUs[i]) {
      index_in_ring = i;
    }
  }
  Logger::getLogger("topology::RingTopology")
      ->info(
          "custom ring, id: {} dimension: {} total nodes in ring: {} index in ring: {}total nodes in ring: {}",
          id,
          name,
          total_nodes_in_ring,
          index_in_ring,
          total_nodes_in_ring);

  assert(index_in_ring >= 0);
}
RingTopology::RingTopology(
    Dimension dimension,
    int id,
    int total_nodes_in_ring,
    int index_in_ring,
    int offset)
    : BasicLogicalTopology(BasicLogicalTopology::BasicTopology::Ring) {
  name = "local";
  if (dimension == Dimension::Vertical) {
    name = "vertical";
  } else if (dimension == Dimension::Horizontal) {
    name = "horizontal";
  }
  if (id == 0) {
    Logger::getLogger("topology::RingTopology")
        ->info(
            "ring of node 0, id: {} dimension: {} total nodes in ring: {} index in ring: {}total nodes in ring: {}",
            id,
            name,
            total_nodes_in_ring,
            index_in_ring,
            offset,
            total_nodes_in_ring);
  }
  this->id = id;
  this->total_nodes_in_ring = total_nodes_in_ring;
  this->index_in_ring = index_in_ring;
  this->dimension = dimension;
  this->offset = offset;

  id_to_index[id] = index_in_ring;
  index_to_id[index_in_ring] = id;
  int tmp = id;
  for (int i = 0; i < total_nodes_in_ring - 1; i++) {
    tmp = get_receiver_homogeneous(
        tmp, RingTopology::Direction::Clockwise, offset);
  }
}

int RingTopology::get_receiver_homogeneous(
    int node_id,
    Direction direction,
    int offset) {
  assert(id_to_index.find(node_id) != id_to_index.end());
  auto logger = Logger::getLogger("topology::RingTopology");
  int index = id_to_index[node_id];
  if (direction == RingTopology::Direction::Clockwise) {
    int receiver = node_id + offset;
    if (index == total_nodes_in_ring - 1) {
      receiver -= (total_nodes_in_ring * offset);
      index = 0;
    } else {
      index++;
    }
    if (receiver < 0) {
      logger->info(
          "at dim: {}at id: {}dimension: {} index: {} ,node id: {} ,offset: {} ,index_in_ring: {} receiver {}",
          name,
          id,
          name,
          index,
          node_id,
          offset,
          index_in_ring,
          receiver);
    }
    assert(receiver >= 0);
    id_to_index[receiver] = index;
    index_to_id[index] = receiver;
    return receiver;
  } else {
    int receiver = node_id - offset;
    if (index == 0) {
      receiver += (total_nodes_in_ring * offset);
      index = total_nodes_in_ring - 1;
    } else {
      index--;
    }
    if (receiver < 0) {
      logger->info(
          "at dim: {}at id: {}dimension: {} index: {} ,node id: {} ,offset: {} ,index_in_ring: {} receiver {}",
          name,
          id,
          name,
          index,
          node_id,
          offset,
          index_in_ring,
          receiver);
    }
    assert(receiver >= 0);
    id_to_index[receiver] = index;
    index_to_id[index] = receiver;
    return receiver;
  }
}

int RingTopology::get_receiver(int node_id, Direction direction) {
  assert(id_to_index.find(node_id) != id_to_index.end());
  int index = id_to_index[node_id];
  if (direction == RingTopology::Direction::Clockwise) {
    index++;
    if (index == total_nodes_in_ring) {
      index = 0;
    }
    return index_to_id[index];
  } else {
    index--;
    if (index < 0) {
      index = total_nodes_in_ring - 1;
    }
    return index_to_id[index];
  }
}

int RingTopology::get_sender(int node_id, Direction direction) {
  assert(id_to_index.find(node_id) != id_to_index.end());
  int index = id_to_index[node_id];
  if (direction == RingTopology::Direction::Anticlockwise) {
    index++;
    if (index == total_nodes_in_ring) {
      index = 0;
    }
    return index_to_id[index];
  } else {
    index--;
    if (index < 0) {
      index = total_nodes_in_ring - 1;
    }
    return index_to_id[index];
  }
}

int RingTopology::get_index_in_ring() {
  return index_in_ring;
}

RingTopology::Dimension RingTopology::get_dimension() {
  return dimension;
}

int RingTopology::get_num_of_nodes_in_dimension(int dimension) {
  return get_nodes_in_ring();
}

int RingTopology::get_nodes_in_ring() {
  return total_nodes_in_ring;
}

bool RingTopology::is_enabled() {
  assert(offset > 0);
  int tmp_index = index_in_ring;
  int tmp_id = id;
  while (tmp_index > 0) {
    tmp_index--;
    tmp_id -= offset;
  }
  if (tmp_id == 0) {
    return true;
  }
  return false;
}
