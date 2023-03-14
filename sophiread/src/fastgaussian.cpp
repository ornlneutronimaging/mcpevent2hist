/**
 * @brief The implemented method is based on
 * https://link.springer.com/chapter/10.1007/978-3-030-96498-6_22
 *
 * @note
 * The algorithm here is an approximation for the gaussian peak fitting, and its
 * accuracy drops for clusters with fewer hits.
 * Generally speaking, for a cluster with larger than 10 hits, it should provide
 * a result (x,y) that is within 1 pixel of the actual centroid.
 * However, fewer data would leads to a larger error.
 */
#include "fastgaussian.h"

#include <Eigen/Dense>
#include <algorithm>
#include <iostream>
#include <numeric>

#include "tpx3.h"

/**
 * @brief Get the Median of a vector of doubles.
 *
 * @param data: a vector of doubles.
 * @return double
 */
double getMedian(const std::vector<double>& data) {
  std::vector<double> sorted_data = data;
  std::sort(sorted_data.begin(), sorted_data.end());
  if (sorted_data.size() % 2 == 0) {
    return (sorted_data[sorted_data.size() / 2 - 1] +
            sorted_data[sorted_data.size() / 2]) /
           2;
  } else {
    return sorted_data[sorted_data.size() / 2];
  }
}

/**
 * @brief Perform gaussian fitting on the hits.
 *
 * @param data: a vector of hits.
 * @return NeutronEvent
 */
NeutronEvent FastGaussian::fit(const std::vector<Hit>& data) {
  // sanity check
  if (data.size() < 8) {
    // need at least 8 data points to fit a gaussian peak with
    // 4 parameters since we are throwing away the bottom 50% of the data
    // points
    // use -1 to indicate that the fit failed
    return NeutronEvent(-1, -1, 0, 0, 0);
  }

  // extract the x, y, tof, and tot into separate vectors
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> tof;
  std::vector<double> tot;
  for (const auto& hit : data) {
    x.push_back((double)DSCALE * hit.getX());
    y.push_back((double)DSCALE * hit.getY());
    tof.push_back((double)hit.getTOF());
    tot.push_back((double)hit.getTOT());
  }

  // calculate the median of tot
  double median_tot = getMedian(tot);

  // subtract the median from tot
  for (auto& t : tot) {
    t -= median_tot;
  }

  // only keep the hits with tot > 0
  std::vector<double> x_filtered;
  std::vector<double> y_filtered;
  std::vector<double> tof_filtered;
  std::vector<double> tot_filtered;
  for (size_t i = 0; i < tot.size(); ++i) {
    if (tot[i] > 0) {
      x_filtered.push_back(x[i]);
      y_filtered.push_back(y[i]);
      tof_filtered.push_back(tof[i]);
      tot_filtered.push_back(tot[i]);
    }
  }

  // vector b = (x^2 + y^2) * tot^2
  Eigen::VectorXd b(x_filtered.size());
  for (size_t i = 0; i < x_filtered.size(); ++i) {
    b(i) = (x_filtered[i] * x_filtered[i] + y_filtered[i] * y_filtered[i]);
  }

  // matrix A = [tot*x, tot*y, tot*log(tot), tot]
  Eigen::MatrixXd A(x_filtered.size(), 4);
  for (size_t i = 0; i < x_filtered.size(); ++i) {
    A(i, 0) = 1.0 * x_filtered[i];
    A(i, 1) = 1.0 * y_filtered[i];
    A(i, 2) = 1.0 * std::log(tot_filtered[i]);
    A(i, 3) = 1.0;
  }

  // solve the linear equation Ax = b with QR decomposition
  // ref: https://docs.w3cub.com/eigen3/group__leastsquares.html
  Eigen::VectorXd x_sol = A.colPivHouseholderQr().solve(b);

  double x_event = x_sol(0) / 2.0;
  double y_event = x_sol(1) / 2.0;

  // calculate the tof as the average of the tof of the filtered hits
  double tof_event =
      std::accumulate(tof_filtered.begin(), tof_filtered.end(), 0.0) /
      tof_filtered.size();

  // calculate the tot
  double tot_event = std::accumulate(tot_filtered.begin(), tot_filtered.end(),0.0);

  // even if we are throwing away to bottom half, we still need to return the
  // pre-filtered number of hits
  return NeutronEvent(x_event, y_event, tof_event, tot_event, tof.size());
}
