/**
 * @file clustering.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Abstract base class for clustering algorithms
 * @version 0.1
 * @date 2023-09-04
 *
 * @copyright Copyright (c) 2023
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <string>
#include <vector>

#include "hit.h"
#include "neutron.h"

/**
 * @brief Abstract class for clustering algorithms
 *
 */
class ClusteringAlgorithm {
 public:
  // set the peak finding method
  virtual void set_method(std::string method) = 0;

  // reset the clustering algorithm
  virtual void reset() = 0;

  // get the cluster labels for each hit
  virtual std::vector<int> get_cluster_labels() = 0;

  // generate cluster IDs for each hit within given vector
  virtual void fit(const std::vector<Hit>& hits) = 0;

  // generate neutron events with given hits and fitted cluster IDs
  virtual std::vector<Neutron> get_events(const std::vector<Hit>& hits) = 0;

  virtual ~ClusteringAlgorithm() {}
};
