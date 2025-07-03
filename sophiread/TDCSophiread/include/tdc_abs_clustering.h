// TDCSophiread ABS (Adaptive Box Search) Clustering Algorithm
// High-performance spatial-temporal clustering for neutron event detection

#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <vector>

#include "tdc_clustering.h"
#include "tdc_clustering_config.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Internal cluster representation for ABS algorithm
 *
 * Fixed-size structure optimized for cache performance.
 * Maintains spatial bounding box and temporal information.
 */
struct ABSCluster {
  int x_min, x_max;    ///< Spatial bounding box (X range)
  int y_min, y_max;    ///< Spatial bounding box (Y range)
  uint32_t timestamp;  ///< Cluster timestamp (25ns units)
  uint16_t size;       ///< Number of hits in cluster
  int32_t label;       ///< Cluster label (-1 = inactive)

  /**
   * @brief Default constructor - creates inactive cluster
   */
  ABSCluster()
      : x_min(0),
        x_max(0),
        y_min(0),
        y_max(0),
        timestamp(0),
        size(0),
        label(-1) {}

  /**
   * @brief Initialize cluster with first hit
   * @param hit Initial hit for the cluster
   * @param cluster_label Unique cluster identifier
   */
  void initialize(const TDCHit& hit, int32_t cluster_label) {
    x_min = x_max = hit.x;
    y_min = y_max = hit.y;
    timestamp = hit.tof;
    size = 1;
    label = cluster_label;
  }

  /**
   * @brief Add hit to existing cluster
   * @param hit Hit to add
   */
  void addHit(const TDCHit& hit) {
    x_min = std::min(x_min, static_cast<int>(hit.x));
    x_max = std::max(x_max, static_cast<int>(hit.x));
    y_min = std::min(y_min, static_cast<int>(hit.y));
    y_max = std::max(y_max, static_cast<int>(hit.y));
    size++;
  }

  /**
   * @brief Check if hit fits spatially within cluster
   * @param hit Hit to test
   * @param radius Maximum distance from cluster boundary
   * @return True if hit is within spatial constraints
   */
  bool fitsSpatially(const TDCHit& hit, double radius) const {
    const int r = static_cast<int>(radius);
    return (hit.x >= x_min - r) && (hit.x <= x_max + r) &&
           (hit.y >= y_min - r) && (hit.y <= y_max + r);
  }

  /**
   * @brief Check if hit fits temporally within cluster
   * @param hit Hit to test
   * @param time_range_ns Maximum time difference in nanoseconds
   * @return True if hit is within temporal constraints
   */
  bool fitsTemporally(const TDCHit& hit, double time_range_ns) const {
    const uint32_t time_range_tdc = static_cast<uint32_t>(time_range_ns / 25.0);
    const uint32_t time_diff =
        (hit.tof > timestamp) ? (hit.tof - timestamp) : (timestamp - hit.tof);
    return time_diff <= time_range_tdc;
  }

  /**
   * @brief Check if cluster is active (has assigned label)
   * @return True if cluster is active
   */
  bool isActive() const { return label >= 0; }

  /**
   * @brief Reset cluster to inactive state
   */
  void reset() {
    label = -1;
    size = 0;
  }
};

/**
 * @brief ABS (Adaptive Box Search) clustering algorithm implementation
 *
 * High-performance spatial-temporal clustering using a fixed pool of 8
 * clusters. Designed for streaming processing of large TPX3 datasets with
 * minimal memory overhead.
 *
 * Algorithm characteristics:
 * - O(n) time complexity (8 clusters max per hit)
 * - Fixed memory footprint independent of dataset size
 * - Cache-optimized cluster pool for maximum performance
 * - LRU replacement strategy for cluster slot management
 * - Supports up to 2.1 billion clusters (int32_t cluster labels)
 * - Built-in overflow protection and bounds checking
 */
class ABSClustering : public IClusteringAlgorithm {
 public:
  /**
   * @brief Constructor with configuration
   * @param config ABS algorithm parameters
   */
  explicit ABSClustering(const ABSConfig& config);

  /**
   * @brief Apply clustering to hit data
   * @param hits Vector of hits to cluster (modified to add cluster labels)
   * @return Number of clusters found
   */
  size_t fit(std::vector<TDCHit>& hits) override;

  /**
   * @brief Get cluster labels assigned during fit()
   * @return Vector of cluster labels (one per hit)
   */
  const std::vector<int>& getClusterLabels() const override;

  /**
   * @brief Get clustering statistics from last fit() call
   * @return Performance and quality metrics
   */
  struct ClusteringStats {
    size_t total_hits;            ///< Number of hits processed
    size_t total_clusters;        ///< Number of clusters created
    size_t single_hit_clusters;   ///< Clusters with only 1 hit
    size_t multi_hit_clusters;    ///< Clusters with multiple hits
    size_t cluster_replacements;  ///< Number of LRU replacements
    double mean_cluster_size;     ///< Average hits per cluster
    double processing_time_ms;    ///< Wall-clock processing time
  };

  /**
   * @brief Get statistics from last clustering operation
   * @return Detailed clustering statistics
   */
  ClusteringStats getStatistics() const;

  /**
   * @brief Reset algorithm state for new dataset
   */
  void reset() override;

  /**
   * @brief Update algorithm configuration
   * @param config New ABS parameters
   */
  void updateConfig(const ABSConfig& config);

  // IClusteringAlgorithm pure virtual method implementations

  /**
   * @brief Configure algorithm with general clustering configuration
   * @param config General clustering configuration (extracts ABS settings)
   */
  void configure(const ClusteringConfig& config) override;

  /**
   * @brief Get algorithm name
   * @return String identifier for this algorithm
   */
  std::string getName() const override;

  /**
   * @brief Get number of hits processed in last fit() call
   * @return Hit count from last clustering operation
   */
  size_t getLastHitCount() const override;

 private:
  // Configuration parameters
  ABSConfig config_;

  // Fixed cluster pool (cache-optimized)
  static constexpr size_t MAX_CLUSTERS = 8;
  std::array<ABSCluster, MAX_CLUSTERS> clusters_;

  // Cluster management
  int32_t next_cluster_label_;  ///< Next available cluster label
  size_t active_clusters_;      ///< Number of currently active clusters

  // Output data
  std::vector<int> cluster_labels_;  ///< Cluster assignment for each hit

  // Statistics
  mutable ClusteringStats stats_;

  // Performance tracking
  mutable std::chrono::high_resolution_clock::time_point start_time_;

  /**
   * @brief Find cluster that can accommodate the hit
   * @param hit Hit to test
   * @return Index of compatible cluster, or -1 if none found
   */
  int findCompatibleCluster(const TDCHit& hit) const;

  /**
   * @brief Find least recently used cluster for replacement
   * @return Index of oldest cluster
   */
  int findOldestCluster() const;

  /**
   * @brief Create new cluster or replace oldest cluster
   * @param hit Hit that starts the new cluster
   * @return Cluster label assigned
   */
  int32_t createOrReplaceCluster(const TDCHit& hit);

  /**
   * @brief Update clustering statistics
   */
  void updateStatistics();
};

}  // namespace tdcsophiread