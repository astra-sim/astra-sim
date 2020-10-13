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

#include "RingTopology.hh"
namespace AstraSim {
RingTopology::RingTopology(Dimension dimension, int id, int total_nodes_in_ring,
                           int index_in_ring, int offset)
    : BasicLogicalTopology(BasicLogicalTopology::BasicTopology::Ring) {
  name = "local";
  if (dimension == Dimension::Vertical) {
    name = "vertical";
  } else if (dimension == Dimension::Horizontal) {
    name = "horizontal";
  }
  if (id == 0) {
    std::cout << "ring of node 0, "
              << "id: " << id << " dimension: " << name
              << " total nodes in ring: " << total_nodes_in_ring
              << " index in ring: " << index_in_ring << " offset: " << offset
              << "total nodes in ring: " << total_nodes_in_ring << std::endl;
  }
  this->id = id;
  this->total_nodes_in_ring = total_nodes_in_ring;
  this->index_in_ring = index_in_ring;
  this->offset = offset;
  this->dimension = dimension;
  find_neighbors();
  id_to_index[id] = index_in_ring;
}
void RingTopology::find_neighbors() {
  this->next_node_id = id + offset;
  if (index_in_ring == total_nodes_in_ring - 1) {
    this->next_node_id -= (total_nodes_in_ring * offset);
    assert(this->next_node_id >= 0);
  }
  previous_node_id = id - offset;
  if (index_in_ring == 0) {
    this->previous_node_id += (total_nodes_in_ring * offset);
    assert(this->previous_node_id >= 0);
  }
}
int RingTopology::get_receiver_node(int node_id, Direction direction) {
  assert(id_to_index.find(node_id) != id_to_index.end());
  int index = id_to_index[node_id];
  // std::cout<<"for node id of"<<node_id<<" at dimension: "<<name<<" index of:
  // "<<index<<" is retrieved"<<std::endl;
  if (direction == RingTopology::Direction::Clockwise) {
    int receiver = node_id + offset;
    if (index == total_nodes_in_ring - 1) {
      receiver -= (total_nodes_in_ring * offset);
      index = 0;
    } else {
      index++;
    }
    if (receiver < 0) {
      std::cout << "at dim: " << name << "at id: " << id
                << "dimension: " << name << " index: " << index
                << " ,node id: " << node_id << " ,offset: " << offset
                << " ,index_in_ring: " << index_in_ring
                << " receiver: " << receiver << std::endl;
    }
    assert(receiver >= 0);
    id_to_index[receiver] = index;
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
      std::cout << "at dim: " << name << "at id: " << id
                << "dimension: " << name << " index: " << index
                << " ,node id: " << node_id << " ,offset: " << offset
                << " ,index_in_ring: " << index_in_ring
                << " receiver: " << receiver << std::endl;
    }
    assert(receiver >= 0);
    id_to_index[receiver] = index;
    return receiver;
  }
}
int RingTopology::get_sender_node(int node_id, Direction direction) {
  assert(id_to_index.find(node_id) != id_to_index.end());
  int index = id_to_index[node_id];
  // std::cout<<"for node id of"<<node_id<<" at dimension: "<<name<<" index of:
  // "<<index<<" is retrieved"<<std::endl;
  if (direction == RingTopology::Direction::Anticlockwise) {
    int sender = node_id + offset;
    if (index == total_nodes_in_ring - 1) {
      sender -= (total_nodes_in_ring * offset);
      index = 0;
    } else {
      index++;
    }
    if (sender < 0) {
      std::cout << "at dim: " << name << " at id: " << id << " index: " << index
                << " ,node id: " << node_id << " ,offset: " << offset
                << " ,index_in_ring: " << index_in_ring
                << " ,sender: " << sender << std::endl;
    }
    assert(sender >= 0);
    id_to_index[sender] = index;
    return sender;
  } else {
    int sender = node_id - offset;
    if (index == 0) {
      sender += (total_nodes_in_ring * offset);
      index = total_nodes_in_ring - 1;
    } else {
      index--;
    }
    if (sender < 0) {
      std::cout << "at dim: " << name << "at id: " << id << "index: " << index
                << " ,node id: " << node_id << " ,offset: " << offset
                << " ,index_in_ring: " << index_in_ring
                << " ,sender: " << sender << std::endl;
    }
    assert(sender >= 0);
    id_to_index[sender] = index;
    return sender;
  }
}
int RingTopology::get_nodes_in_ring() { return total_nodes_in_ring; }
bool RingTopology::is_enabled() {
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
} // namespace AstraSim