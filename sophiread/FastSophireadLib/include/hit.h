/**
 * @file hit.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Hit definition
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
#pragma once

#include <sstream>
#include <string>

class Hit {
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