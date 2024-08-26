/**
 * @file centroid.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Class for the Centroid algorithm
 * @version 0.1
 * @date 2023-09-04
 *
 * @copyright Copyright (c) 2023
 * SPDX - License - Identifier: GPL - 3.0 +
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
      : m_weighted_by_tot(weighted_by_tot),
        m_super_resolution_factor(super_resolution_factor){};

  void set_weighted_by_tot(bool weighted_by_tot) {
    m_weighted_by_tot = weighted_by_tot;
  }

  void set_super_resolution_factor(double super_resolution_factor) {
    m_super_resolution_factor = super_resolution_factor;
  }

  // Pure virtual function for predicting the peak positions and parameters
  // predict -> (x, y, tof)
  Neutron fit(const std::vector<Hit>& data) override;

 private:
  bool m_weighted_by_tot = true;
  double m_super_resolution_factor =
      1.0;  // it is better to perform super resolution during post processing,
            // but it can also be part of the fitting algorithm
};
