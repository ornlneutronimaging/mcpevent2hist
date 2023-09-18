/**
 * @file abs.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Implementation of ABS class
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

#include "abs.h"

#include "centroid.h"
#include "fastgaussian.h"
#include "spdlog/spdlog.h"

/**
 * @brief Generate cluster labels for the hits.
 *
 * @param[in] data: a vector of hits.
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
 * @param[in] data: a vector of hits.
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
  int max_label = -1;
  if (clusterLabels_.empty()) {
    return events;
  } else {
    auto max_label_it = std::max_element(clusterLabels_.begin(), clusterLabels_.end());
    max_label = *max_label_it;
  }

  // determine fitting algorithm
  if (m_method == "centroid") {
    m_alg = std::make_unique<Centroid>(true);
  } else if (m_method == "fast_gaussian") {
    m_alg = std::make_unique<FastGaussian>();
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
    auto event = m_alg->fit(cluster);

    // Add the event to the list
    if (event.getX() >= 0.0 && event.getY() >= 0.0) {
      // x, y = -1 means a failed fit
      events.push_back(event);
    }
  }

  return events;
}
