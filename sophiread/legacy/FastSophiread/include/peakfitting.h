/**
 * @file peakfitting.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Abstract base class for peak fitting algorithms
 * @version 0.1
 * @date 2023-09-04
 *
 * @copyright Copyright (c) 2023
 * SPDX - License - Identifier: GPL - 3.0 +
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