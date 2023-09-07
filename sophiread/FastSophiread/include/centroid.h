/**
 * @file centroid.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Class for the Centroid algorithm
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

#include "peakfitting.h"

/**
 * @brief Using weighted centroid to predict the peak position
 *
 * x_peak = sum(x_i * tot_i) / sum(tot_i)
 * y_peak = sum(y_i * tot_i) / sum(tot_i)
 * tof_peak = sum(tof_i) / N
 *
 * NOTE: TOF distribution within a cluster should be small enough that algebraic
 * mean is sufficient to approximating the TOF of the incident neutron.
 */
class Centroid : public PeakFittingAlgorithm {
 public:
  Centroid(bool weighted_by_tot = true) : m_weighted_by_tot(weighted_by_tot){};
  Centroid(bool weighted_by_tot, double super_resolution_factor)
      : m_weighted_by_tot(weighted_by_tot), m_super_resolution_factor(super_resolution_factor){};

  void set_weighted_by_tot(bool weighted_by_tot) { m_weighted_by_tot = weighted_by_tot; }

  void set_super_resolution_factor(double super_resolution_factor) {
    m_super_resolution_factor = super_resolution_factor;
  }

  // Pure virtual function for predicting the peak positions and parameters
  // predict -> (x, y, tof)
  Neutron fit(const std::vector<Hit>& data) override;

 private:
  bool m_weighted_by_tot = true;
  double m_super_resolution_factor = 1.0;  // it is better to perform super resolution during post processing, but it
                                           // can also be part of the fitting algorithm
};
