/**
 * @file neutron.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Neutron event definition
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
#pragma once

#include <sstream>
#include <vector>

class Neutron {
 public:
  Neutron(const double x, const double y, const double tof, const double tot, const int nHits)
      : m_x(x), m_y(y), m_tof(tof), m_tot(tot), m_nHits(nHits){};

  double getX() const { return m_x; };
  double getY() const { return m_y; };
  double getTOT() const { return m_tot; }
  double getTOF() const { return m_tof; };
  double getTOF_ns() const { return m_tof * m_scale_to_ns_40mhz; };
  int getNHits() const { return m_nHits; };

  std::string toString() const {
    std::stringstream ss;
    ss << "Neutron: x= " << m_x << ", y= " << m_y << ", tof= " << m_tof << ", nHits= " << m_nHits;
    return ss.str();
  };

 private:
  double m_x, m_y;                    // pixel coordinates
  double m_tof;                       // time of flight
  double m_tot;                       // time-over-threshold
  int m_nHits;                        // number of hits in the event (cluster size)
  double m_scale_to_ns_40mhz = 25.0;  // 40 MHz clock is used for the coarse time of arrival.
};