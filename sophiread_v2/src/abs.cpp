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
  // reserve space for the cluster labels and initialize to -1
  clusterLabels_.clear();
  clusterLabels_.resize(data.size(), -1);

  // make a vector of clusters
  std::vector<Cluster> clusters;
  for (int i = 0; i < numClusters_; i++) {
    clusters.push_back(Cluster{0, 0, 0, 0, i, 0});
  }

  // loop over all hits
  // NOTE: this section can use openMP to speed up by chunking hits into smaller
  //       groups
  int max_label = numClusters_;
  std::vector<int> cluster_sizes(numClusters_, 0);
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
      // case_1: cluster is not full, check if hit is within the feather range
      // of the cluster, if yes, add hit to cluster and update cluster bounds,
      // if not, id as noise
      else if (cluster.size < maxClusterSize_) {
        if (hit.getX() >= cluster.x_min - m_feature &&
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
        } else {
          continue;
        }
      }
    }
    //
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
    std::cout << clusterLabels_.size() << " " << data.size() << std::endl;
    throw std::runtime_error("ERROR: cluster labels size does not match data!");
  }

  // Group data based on cluster labels
  std::vector<NeutronEvent> events;
  // find the highest label
  auto max_label_it =
      std::max_element(clusterLabels_.begin(), clusterLabels_.end());
  int max_label = *max_label_it;
  // #pragma omp parallel for
  for (int label = 0; label <= max_label; label++) {
    std::vector<Hit> cluster;
    for (size_t i = 0; i < data.size(); i++) {
      if (clusterLabels_[i] == label) {
        cluster.push_back(data[i]);
      }
    }
    // get the neutron event
    PeakFittingAlgorithm* alg;
    if (m_method == "centroid") {
      alg = new Centroid(true);
    } else if (m_method == "gaussian") {
      alg = new FastGaussian();
    } else {
      throw std::runtime_error("ERROR: peak fitting method not supported!");
    }
    auto event = alg->fit(cluster);
    // Add the event to the list
    // #pragma omp critical
    events.push_back(event);
  }

  return events;
}
