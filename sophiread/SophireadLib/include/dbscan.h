#pragma once

#include "clustering.h"

class DBSCAN : public ClusteringAlgorithm {
 public:
  DBSCAN(double eps_time, size_t min_points_time, double eps_xy,
         size_t min_points_xy)
      : m_eps_time(eps_time),
        m_min_points_time(min_points_time),
        m_eps_xy(eps_xy),
        m_min_points_xy(min_points_xy){};
  ~DBSCAN() = default;

 public:
  void set_method(std::string method) { m_method = method; };
  void reset();
  std::vector<int> get_cluster_labels();
  void fit(const std::vector<Hit> &hits);
  std::vector<NeutronEvent> get_events(const std::vector<Hit> &hits);
  bool verbose() const { return m_verbose; }
  void set_verbose(bool verbose) { m_verbose = verbose; }

 private:
  class TimeClusterInfo {
   public:
    TimeClusterInfo();
    TimeClusterInfo(const double time, const size_t xy_index);
    ~TimeClusterInfo() = default;

   public:
    double m_time_mean;
    double m_time_sum;
    double m_time_min;
    double m_time_max;
    std::vector<size_t> m_time_cluster_xy_indexes;  // wrt input hits vector
  };
  void fit1D(std::vector<double> &data, size_t &number_of_clusters,
             std::vector<size_t> &labels, std::vector<double> &centroids);
  void fit2D(std::vector<std::pair<double, double>> &data,
             size_t &number_of_clusters, std::vector<size_t> &labels,
             std::vector<std::pair<double, double>> &centroids);
  void mergeTimeClusters1D(std::vector<TimeClusterInfo> &input_infos,
                           std::vector<TimeClusterInfo> &merged_infos);

 private:
  std::string m_method{"centroid"};  // method for centroid
  double m_eps_time;         // The maximum distance between two time points
  size_t m_min_points_time;  // The minimum number of points in a time cluster
  double m_eps_xy;           // The maximum distance between two XY points
  size_t m_min_points_xy;    // The minimum number of points in an XY cluster
  std::vector<NeutronEvent> m_events;
  const size_t m_max_hit_chunk_size = 2e6;
  std::vector<int> clusterLabels_;  // The cluster labels for each hit
  bool m_verbose = false;
};
