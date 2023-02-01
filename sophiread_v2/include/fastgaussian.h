#include "peakfitting.h"

class FastGaussian : public PeakFittingAlgorithm {
 public:
  FastGaussian(){};

  // Pure virtual function for predicting the peak positions and parameters
  // predict -> (x, y, tof)
  NeutronEvent fit(const std::vector<Hit>& data) override;
};