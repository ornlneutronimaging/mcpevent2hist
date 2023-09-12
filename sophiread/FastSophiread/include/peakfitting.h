/**
 * @file peakfitting.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Abstract base class for peak fitting algorithms
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

#include <vector>

#include "hit.h"
#include "neutron.h"

/**
 * @brief Abstract base class for peak fitting algorithms.
 *
 */
class PeakFittingAlgorithm {
 public:
  // Pure virtual function for predicting the peak positions and parameters
  // predict -> (x, y, tof)
  virtual Neutron fit(const std::vector<Hit>& data) = 0;

  // Virtual destructor for proper cleanup
  virtual ~PeakFittingAlgorithm() {}
};