#include <tuple>

#include "peakfitting.h"

class Centroid : public PeakFittingAlgorithm {
 public:
  Centroid(bool weighted_by_tot = true) : weighted_by_tot(weighted_by_tot){};

  // Pure virtual function for initializing the algorithm
  void initialize() override;

  // Pure virtual function for fitting the data
  void fit(const std::vector<Hit>& data) override;

  // Pure virtual function for predicting the peak positions and parameters
  // predict -> (x, y, tof)
  std::vector<std::tuple<double, double, double>> predict(
      const std::vector<Hit>& data) override;

 private:
  bool weighted_by_tot = true;
};
