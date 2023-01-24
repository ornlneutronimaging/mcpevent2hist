#pragma once
#include <vector>

#include "tpx3.h"

class PeakFittingAlgorithm {
 public:
  // Pure virtual function for initializing the algorithm
  virtual void initialize() = 0;

  // Pure virtual function for fitting the data
  virtual void fit(const std::vector<Hit>& data) = 0;

  // Pure virtual function for predicting the peak positions and parameters
  // predict -> (x, y, tof)
  virtual std::vector<std::tuple<double, double, double>> predict(
      const std::vector<Hit>& data) = 0;

  // Virtual destructor for proper cleanup
  virtual ~PeakFittingAlgorithm() {}
};