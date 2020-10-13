/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "Torus3D.hh"
namespace AstraSim {
Torus3D::Torus3D(int id, int total_nodes, int local_dim, int vertical_dim) {
  int horizontal_dim = total_nodes / (vertical_dim * local_dim);
  local_dimension = new RingTopology(
      RingTopology::Dimension::Local, id, local_dim, id % local_dim, 1);
  vertical_dimension = new RingTopology(
      RingTopology::Dimension::Vertical,
      id,
      vertical_dim,
      id / (local_dim * horizontal_dim),
      local_dim * horizontal_dim);
  horizontal_dimension = new RingTopology(
      RingTopology::Dimension::Horizontal,
      id,
      horizontal_dim,
      (id / local_dim) % horizontal_dim,
      local_dim);
}
Torus3D::~Torus3D() {
  delete local_dimension;
  delete vertical_dimension;
  delete horizontal_dimension;
};
} // namespace AstraSim