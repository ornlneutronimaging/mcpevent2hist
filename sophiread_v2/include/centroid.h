#include <tuple>

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
  Centroid(bool weighted_by_tot = true) : weighted_by_tot(weighted_by_tot){};

  // Pure virtual function for predicting the peak positions and parameters
  // predict -> (x, y, tof)
  NeutronEvent fit(const std::vector<Hit>& data) override;

 private:
  bool weighted_by_tot = true;
};
