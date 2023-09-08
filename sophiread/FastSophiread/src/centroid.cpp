/**
 * @file centroid.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Implementation of Centroid peak fitting method.
 * @version 0.1
 * @date 2023-09-04
 *
 * @copyright Copyright (c) 2023
 * BSD 3-Clause License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of ORNL nor the names of its contributors may be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "centroid.h"

/**
 * @brief Perform centroid fitting on the hits.
 *
 * @param data: a vector of hits.
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
