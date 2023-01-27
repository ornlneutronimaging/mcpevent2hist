#include "peakfitting.h"

class FastGaussian : public PeakFittingAlgorithm {
 public:
  FastGaussian(bool weighted_by_tot = true)
      : weighted_by_tot(weighted_by_tot){};

  // Pure virtual function for predicting the peak positions and parameters
  // predict -> (x, y, tof)
  NeutronEvent fit(const std::vector<Hit>& data) override;

 private:
  bool weighted_by_tot = true;
};