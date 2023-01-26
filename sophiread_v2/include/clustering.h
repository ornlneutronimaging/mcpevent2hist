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

  // generate neutron events with given hits and fitted cluster IDs
  virtual std::vector<NeutronEvent> get_events(
      const std::vector<Hit>& hits) = 0;
};
