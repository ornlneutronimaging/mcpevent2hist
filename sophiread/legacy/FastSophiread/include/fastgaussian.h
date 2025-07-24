/**
 * @file fastgaussian.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Class for the Fast Gaussian algorithm
 * @version 0.1
 * @date 2023-09-04
 *
 * @copyright Copyright (c) 2023
 * SPDX - License - Identifier: GPL - 3.0 +
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