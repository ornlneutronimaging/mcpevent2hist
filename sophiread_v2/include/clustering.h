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

  // generate cluster IDs for each hit within given vector
  virtual void fit(const std::vector<Hit>& hits) = 0;

  // predict the clusters
  virtual std::vector<NeutronEvent> predict(const std::vector<Hit>& hits) = 0;
};
