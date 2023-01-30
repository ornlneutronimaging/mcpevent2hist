#include "abs.h"

#include <omp.h>

#include <algorithm>
#include <iostream>

#include "centroid.h"
#include "fastgaussian.h"

/**
 * @brief Generate cluster labels for the hits
 *
 * @param data
 */
void ABS::fit(const std::vector<Hit>& data) {
  // reserve space for the cluster labels
  clusterLabels_.reserve(data.size());

  // make a vector of clusters
  std::vector<Cluster> clusters;
  for (int i = 0; i < numClusters_; i++) {
    clusters.push_back(Cluster{0, 0, 0, 0, i, 0});
  }

  // loop over all hits
  // NOTE: this section can use openMP to speed up by chunking hits into smaller
  //       groups
  int max_label = numClusters_;
  for (size_t i = 0; i < data.size(); i++) {
    auto& hit = data[i];
    int label = -1;  // assume it is a noise
    // loop over all clusters
    for (auto& cluster : clusters) {
      // case_0: cluster is empty, add hit to cluster and update cluster bounds
      if (cluster.size == 0) {
        cluster.size++;
        cluster.x_min = hit.getX();
        cluster.x_max = hit.getX();
        cluster.y_min = hit.getY();
        cluster.y_max = hit.getY();
        //
        label = cluster.label;
        break;
      }
      // case_1: cluster is full, reset cluster, increase the label and
      // max_label, and add hit to cluster
      else if (cluster.size == maxClusterSize_) {
        cluster.size = 1;
        cluster.x_min = hit.getX();
        cluster.x_max = hit.getX();
        cluster.y_min = hit.getY();
        cluster.y_max = hit.getY();
        cluster.label = max_label;
        max_label++;
        //
        label = cluster.label;
        break;
      }
      // case_2: hit is within the feather range of the cluster, add hit to
      // cluster and update cluster bounds
      else if (hit.getX() >= cluster.x_min - m_feature &&
               hit.getX() <= cluster.x_max + m_feature &&
               hit.getY() >= cluster.y_min - m_feature &&
               hit.getY() <= cluster.y_max + m_feature) {
        cluster.size++;
        cluster.x_min = std::min(cluster.x_min, hit.getX());
        cluster.x_max = std::max(cluster.x_max, hit.getX());
        cluster.y_min = std::min(cluster.y_min, hit.getY());
        cluster.y_max = std::max(cluster.y_max, hit.getY());
        //
        label = cluster.label;
        break;
      }
      // case_3: hit is not within the feather range of the cluster, id as noise
      else {
        continue;
      }
    }
    clusterLabels_[i] = label;
  }
}

/**
 * @brief Predict the clusters by retrieving the labels of the hits
 *
 * @param data
 * @return std::vector<int>
 */
std::vector<NeutronEvent> ABS::get_events(const std::vector<Hit>& data) {
  // Sanity check
  if (clusterLabels_.size() != data.size()) {
    std::cout << "ERROR: cluster labels size does not match data size, call "
                 "fit first!"
              << std::endl;
    return std::vector<NeutronEvent>();
  }

  // Create peak fitting algorithm
  if (m_method == "centroid") {
    peakFittingAlgorithm_ = new Centroid(true);
  } else if (m_method == "gaussian") {
    peakFittingAlgorithm_ = new FastGaussian();
  } else {
    std::cout << "ERROR: peak fitting method not supported!" << std::endl;
    return std::vector<NeutronEvent>();
  }

  // Group data based on cluster labels
  std::vector<NeutronEvent> events;
  // find the highest label
  auto max_label_it =
      std::max_element(clusterLabels_.begin(), clusterLabels_.end());
  int max_label = *max_label_it;
#pragma omp parallel for
  for (int label = 0; label <= max_label; label++) {
    std::vector<Hit> cluster;
    for (size_t i = 0; i < data.size(); i++) {
      if (clusterLabels_[i] == label) {
        cluster.push_back(data[i]);
      }
    }
    // get the neutron event
    NeutronEvent event = peakFittingAlgorithm_->fit(cluster);
    // Add the event to the list
#pragma omp critical
    events.push_back(event);
  }

  return events;
}
