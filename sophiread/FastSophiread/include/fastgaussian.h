/**
 * @file fastgaussian.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Class for the Fast Gaussian algorithm
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
 * @brief Peak fitting algorithm using a fast gaussian fit via least squares.
 *
 */
class FastGaussian : public PeakFittingAlgorithm {
 public:
  FastGaussian(){};
  FastGaussian(double super_resolution_factor)
      : m_super_resolution_factor(super_resolution_factor){};

  void set_super_resolution_factor(double super_resolution_factor) {
    m_super_resolution_factor = super_resolution_factor;
  }

  // Pure virtual function for predicting the peak positions and parameters
  // predict -> (x, y, tof)
  Neutron fit(const std::vector<Hit>& data) override;

 private:
  double m_super_resolution_factor = 1.0;  // super resolution factor
};