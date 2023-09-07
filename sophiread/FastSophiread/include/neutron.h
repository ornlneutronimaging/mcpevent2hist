/**
 * @file neutron.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Neutron event definition
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
  const double m_x, m_y;                    // pixel coordinates
  const double m_tof;                       // time of flight
  const double m_tot;                       // time-over-threshold
  const int m_nHits;                        // number of hits in the event (cluster size)
  const double m_scale_to_ns_40mhz = 25.0;  // 40 MHz clock is used for the coarse time of arrival.
};