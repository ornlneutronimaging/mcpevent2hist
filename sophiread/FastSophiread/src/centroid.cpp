/**
 * @file centroid.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Implementation of Centroid peak fitting method.
 * @version 0.1
 * @date 2023-09-04
 *
 * @copyright Copyright (c) 2023
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "centroid.h"

/**
 * @brief Perform centroid fitting on the hits.
 *
 * @param[in] data: a vector of hits.
 * @return NeutronEvent: a neutron event.
 */
Neutron Centroid::fit(const std::vector<Hit>& data) {
  double x = 0;
  double y = 0;
  double tof = 0;
  double tot = 0;

  if (data.size() == 0) {
    return Neutron(0, 0, 0, 0, 0);
  }

  if (m_weighted_by_tot) {
    for (const auto& hit : data) {
      const auto hit_x = hit.getX();
      const auto hit_y = hit.getY();
      const auto hit_tot = hit.getTOT();
      const auto hit_tof = hit.getTOF();

      x += m_super_resolution_factor * hit_x * hit_tot;
      y += m_super_resolution_factor * hit_y * hit_tot;
      tof += hit_tof;
      tot += hit_tot;
    }
    const auto tot_inv = 1.0 / tot;
    x *= tot_inv;
    y *= tot_inv;
  } else {
    for (const auto& hit : data) {
      x += m_super_resolution_factor * hit.getX();
      y += m_super_resolution_factor * hit.getY();
      tof += hit.getTOF();
      tot += hit.getTOT();
    }
    const auto data_size_inv = 1.0 / data.size();
    x *= data_size_inv;
    y *= data_size_inv;
  }

  tof /= data.size();

  return Neutron(x, y, tof, tot, data.size());
}
