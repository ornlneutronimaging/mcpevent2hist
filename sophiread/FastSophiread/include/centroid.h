/**
 * @file centroid.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Class for the Centroid algorithm
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
