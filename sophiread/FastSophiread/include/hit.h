/**
 * @file hit.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Hit definition
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
#include <string>

#include "iposition_tof.h"

class Hit : public IPositionTOF {
 public:
  // default constructor
  Hit() : m_x(0), m_y(0), m_tot(0), m_toa(0), m_ftoa(0), m_tof(0), m_spidertime(0){};
  // copy constructor
  Hit(const Hit& hit)
      : m_x(hit.m_x),
        m_y(hit.m_y),
        m_tot(hit.m_tot),
        m_toa(hit.m_toa),
        m_ftoa(hit.m_ftoa),
        m_tof(hit.m_tof),
        m_spidertime(hit.m_spidertime){};

  Hit(int x, int y, int tot, int toa, int ftoa, unsigned int tof, unsigned long long spidertime)
      : m_x(x), m_y(y), m_tot(tot), m_toa(toa), m_ftoa(ftoa), m_tof(tof), m_spidertime(spidertime){};

  // special constructor that directly parse the raw packet from tpx3
  // into a hit
  Hit(const char* packet, const unsigned long long tdc, const unsigned long long gdc, const int chip_layout_type);

  Hit& operator=(const Hit& hit) {
    m_x = hit.m_x;
    m_y = hit.m_y;
    m_tot = hit.m_tot;
    m_toa = hit.m_toa;
    m_ftoa = hit.m_ftoa;
    m_tof = hit.m_tof;
    m_spidertime = hit.m_spidertime;
    return *this;
  }

  int getX() const { return m_x; };
  int getY() const { return m_y; };
  int getTOT() const { return m_tot; };
  int getTOA() const { return m_toa; };
  int getFTOA() const { return m_ftoa; };
  unsigned long long getSPIDERTIME() const { return m_spidertime; };
  unsigned int getTOF() const { return m_tof; };

  double getTOF_ns() const { return m_tof * m_scale_to_ns_40mhz; };
  double getTOA_ns() const { return m_toa * m_scale_to_ns_40mhz; };
  double getTOT_ns() const { return m_tot * m_scale_to_ns_40mhz; };
  double getSPIDERTIME_ns() const { return m_spidertime * m_scale_to_ns_40mhz; };
  double getFTOA_ns() const { return m_ftoa * m_scale_to_ns_640mhz; };

  std::string toString() const {
    std::stringstream ss;
    ss << "Hit: x=" << m_x << ", y=" << m_y << ", tot=" << m_tot << ", toa=" << m_toa << ", ftoa=" << m_ftoa
       << ", tof=" << m_tof << ", spidertime=" << m_spidertime;
    return ss.str();
  };

  // Implement the interface methods
  double iGetX() const override { return static_cast<double>(getX()); }
  double iGetY() const override { return static_cast<double>(getY()); }
  double iGetTOF_ns() const override { return getTOF_ns(); }

 private:
  // raw packet directly read from tpx3.
  int m_x, m_y;  // pixel coordinates
  int m_tot;     // time over threshold
  int m_toa;     // time of arrival (40MHz clock, 14 bit)
  int m_ftoa;    // fine time of arrival (640MHz clock, 4 bit)
  unsigned int m_tof;
  unsigned long long m_spidertime;  // time from the spider board (in the unit of 25ns)

  // scale factor that converts time to ns
  const double m_scale_to_ns_40mhz = 25.0;          // 40 MHz clock is used for the coarse time of arrival.
  const double m_scale_to_ns_640mhz = 25.0 / 16.0;  // 640 MHz clock is used for the fine time of arrival.
};