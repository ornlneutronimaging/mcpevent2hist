/**
 * @file abs.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Implementation of ABS class
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

#include "abs.h"

#include "centroid.h"
#include "fastgaussian.h"
#include "spdlog/spdlog.h"

/**
 * @brief Generate cluster labels for the hits.
 *
 * @param data: a vector of hits.
 */
void ABS::fit(const std::vector<Hit>& data) {
  // reserve space for the cluster labels and initialize to -1
  clusterLabels_.clear();
  clusterLabels_.resize(data.size(), -1);

  // make a vector of clusters
  std::vector<Cluster> clusters;
  for (int i = 0; i < numClusters_; i++) {
    clusters.push_back(Cluster{0, 0, 0, 0, 0, i, 0});
  }

  // loop over all hits
  // NOTE: this section can use openMP to speed up by chunking hits into smaller
  //       groups
  int max_label = numClusters_;
  std::vector<int> cluster_sizes(numClusters_, 0);
  for (size_t i = 0; i < data.size(); i++) {
    auto& hit = data[i];
    int label = -1;  // assume it is a noise
    bool foundCluster = false;

    // loop over all clusters
    for (auto& cluster : clusters) {
      // case_0: cluster is empty, add hit to cluster and update cluster bounds
      if (cluster.size == 0) {
        cluster.size++;
        cluster.x_min = hit.getX();
        cluster.x_max = hit.getX();
        cluster.y_min = hit.getY();
        cluster.y_max = hit.getY();
        cluster.spidertime = hit.getSPIDERTIME_ns();
        //
        label = cluster.label;
        foundCluster = true;
        break;
      }
      // case_1: cluster is not full, check if hit is within the feather range
      // of the cluster, if yes, add hit to cluster and update cluster bounds,
      // if not, id as noise
      else if (std::abs(hit.getSPIDERTIME_ns() - cluster.spidertime) <= spiderTimeRange_) {
        if (hit.getX() >= cluster.x_min - m_feature && hit.getX() <= cluster.x_max + m_feature &&
            hit.getY() >= cluster.y_min - m_feature && hit.getY() <= cluster.y_max + m_feature) {
          cluster.size++;
          cluster.x_min = std::min(cluster.x_min, hit.getX());
          cluster.x_max = std::max(cluster.x_max, hit.getX());
          cluster.y_min = std::min(cluster.y_min, hit.getY());
          cluster.y_max = std::max(cluster.y_max, hit.getY());
          //
          label = cluster.label;
          foundCluster = true;
          break;
        } else {
          continue;
        }
      }
    }

    if (!foundCluster) {
      // look for a cluster with the smallest spidertime
      auto min_spidertime = clusters[0].spidertime;
      int min_spidertime_index = 0;
      for (int j = 1; j < numClusters_; j++) {
        if (clusters[j].size > 0 && clusters[j].spidertime < min_spidertime) {
          min_spidertime = clusters[j].spidertime;
          min_spidertime_index = j;
        }
      }

      // reset the cluster with the smallest spidertime, and add hit to cluster
      // and update cluster bounds
      clusters[min_spidertime_index].size = 1;
      clusters[min_spidertime_index].x_min = hit.getX();
      clusters[min_spidertime_index].x_max = hit.getX();
      clusters[min_spidertime_index].y_min = hit.getY();
      clusters[min_spidertime_index].y_max = hit.getY();
      clusters[min_spidertime_index].spidertime = hit.getSPIDERTIME_ns();
      clusters[min_spidertime_index].label = max_label;
      max_label++;
      //
      label = clusters[min_spidertime_index].label;
    }

    // update label
    clusterLabels_[i] = label;
  }
  // convert clusterLabels_ to a 2D list of index
  clusterIndices_.clear();
  clusterIndices_.resize(max_label + 1);
  for (size_t i = 0; i < clusterLabels_.size(); i++) {
    clusterIndices_[clusterLabels_[i]].push_back(i);
  }
}

/**
 * @brief Predict the clusters by retrieving the labels of the hits.
 *
 * @param data: a vector of hits.
 * @return std::vector<NeutronEvent>: a vector of neutron events.
 */
std::vector<Neutron> ABS::get_events(const std::vector<Hit>& data) {
  // Sanity check
  if (clusterLabels_.size() != data.size()) {
    spdlog::critical("ERROR: cluster labels size does not match data!");
    throw std::runtime_error("ERROR: cluster labels size does not match data!");
  }

  // Group data based on cluster labels
  std::vector<Neutron> events;

  // find the highest label
  auto max_label_it = std::max_element(clusterLabels_.begin(), clusterLabels_.end());
  int max_label = *max_label_it;

  // determine fitting algorithm
  std::unique_ptr<PeakFittingAlgorithm> alg;
  if (m_method == "centroid") {
    alg = std::make_unique<Centroid>(true);
  } else if (m_method == "fast_gaussian") {
    alg = std::make_unique<FastGaussian>();
  } else {
    spdlog::critical("ERROR: peak fitting method not supported!");
    throw std::runtime_error("ERROR: peak fitting method not supported!");
  }

  // pre-allocate memory for events
  events.reserve(max_label + 1);

  // loop over all clusterIndices_
  for (int label = 0; label <= max_label; label++) {
    std::vector<Hit> cluster;
    cluster.reserve(clusterIndices_[label].size());

    for (auto& index : clusterIndices_[label]) {
      cluster.push_back(data[index]);
    }
    if (cluster.size() < m_min_cluster_size) {
      continue;
    }

    // get the neutron event
    auto event = alg->fit(cluster);

    // Add the event to the list
    if (event.getX() >= 0.0 && event.getY() >= 0.0) {
      // x, y = -1 means a failed fit
      events.push_back(event);
    }
  }

  return events;
}
