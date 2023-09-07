/**
 * @file abs.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Class for the AdaptiveBoxSearch (ABS) algorithm
 * @version 0.1
 * @date 2023-09-04
 *
 * @copyright Copyright (c) 2023
 * BSD 3-Clause License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of ORNL nor the names of its contributors may be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <memory>

#include "clustering.h"
#include "peakfitting.h"

/**
 * @brief Struct for the cluster of hits (charged particles).
 *
 */
struct Cluster {
  int x_min, y_min, x_max, y_max;
  unsigned long long spidertime;
  int label, size;
};

/**
 * @brief Class for the AdaptiveBoxSearch (ABS) algorithm
 *
 */
class ABS : public ClusteringAlgorithm {
 public:
  ABS(double r, unsigned long int min_cluster_size, unsigned long int spider_time_range)
      : m_feature(r), m_min_cluster_size(min_cluster_size), spiderTimeRange_(spider_time_range){};
  void fit(const std::vector<Hit>& data);
  void set_method(std::string method) { m_method = method; }
  void reset() { clusterLabels_.clear(); }
  std::vector<int> get_cluster_labels() { return clusterLabels_; }
  std::vector<Neutron> get_events(const std::vector<Hit>& data);
  ~ABS() = default;

 private:
  double m_feature;                               // feather range
  std::string m_method{"centroid"};               // method for centroid
  std::vector<int> clusterLabels_;                // The cluster labels for each hit
  std::vector<std::vector<int>> clusterIndices_;  // The cluster indices for
                                                  // each cluster
  const int numClusters_ = 8;                     // The number of clusters use in runtime
  unsigned long int m_min_cluster_size = 1;       // The maximum cluster size
  unsigned long int spiderTimeRange_ = 75;        // The spider time range (in ns)
  std::unique_ptr<PeakFittingAlgorithm> m_alg;    // The clustering algorithm
};
