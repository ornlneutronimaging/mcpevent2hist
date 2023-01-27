#pragma once
#include <vector>

#include "tpx3.h"

class PeakFittingAlgorithm {
 public:
  // Pure virtual function for predicting the peak positions and parameters
  // predict -> (x, y, tof)
  virtual NeutronEvent fit(const std::vector<Hit>& data) = 0;

  // Virtual destructor for proper cleanup
  virtual ~PeakFittingAlgorithm() {}
};