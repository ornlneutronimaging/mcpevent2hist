#pragma once

#include <vector>

#include "tpx3.h"

/**
 * @brief Abstract class for clustering algorithms
 *
 */
class ClusteringAlgorithm {
  // initialize the algorithm
  virtual void initialize() = 0;

  // fit the algorithm to the data
  virtual void fit(const std::vector<Hit>& hits) = 0;

  // predict the clusters
  virtual std::vector<int> predict(const std::vector<Hit>& hits) = 0;
};
