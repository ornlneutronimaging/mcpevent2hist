#pragma once

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
  ABS(double r) : m_feature(r){};
  void fit(const std::vector<Hit>& data);
  void set_method(std::string method) { m_method = method; };
  void reset() { clusterLabels_.clear(); };
  std::vector<int> get_cluster_labels() { return clusterLabels_; }
  std::vector<NeutronEvent> get_events(const std::vector<Hit>& data);
  ~ABS() = default;

 private:
  double m_feature;                  // feather range
  std::string m_method{"centroid"};  // method for centroid
  std::vector<int> clusterLabels_;   // The cluster labels for each hit
  std::vector<std::vector<int>> clusterIndices_;  // The cluster indices for
                                                  // each cluster
  const int numClusters_ = 128;     // The number of clusters use in runtime
  const int m_min_cluster_size = 3;  // The maximum cluster size
  const int spiderTimeRange_ = 75;  // The spider time range (in ns)
  PeakFittingAlgorithm* peakFittingAlgorithm_;  // The clustering algorithm
};
