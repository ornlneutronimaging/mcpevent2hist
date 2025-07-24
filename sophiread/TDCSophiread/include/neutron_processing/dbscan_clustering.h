// TDCSophiread DBSCAN Clustering
// Density-based clustering implementation for temporal window processing
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "neutron_processing/clustering_state.h"
#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

// Forward declaration
class DBSCANClusteringState;

/**
 * @brief DBSCAN (Density-Based Spatial Clustering) implementation (stateless)
 *
 * Implements density-based clustering where clusters are formed from
 * dense regions of points. Points in low-density regions are marked as noise.
 *
 * This implementation is designed for the temporal window architecture,
 * supporting stateless operation for safe parallel processing.
 *
 * Algorithm parameters:
 * - epsilon: Maximum distance between points to be considered neighbors
 * - minPts: Minimum number of points to form a dense region
 */
class DBSCANHitClustering : public IHitClustering {
 private:
  // Configuration (immutable after construction)
  HitClusteringConfig config_;

  // Spatial grid parameters for efficient neighbor search
  static constexpr size_t SPATIAL_GRID_SIZE = 64;

  /**
   * @brief Find all neighbors within epsilon distance
   * @param hit_idx Index of the hit to find neighbors for
   * @param begin Iterator to first hit
   * @param end Iterator to last hit
   * @param state DBSCAN clustering state
   * @return Vector of neighbor indices
   */
  std::vector<size_t> findNeighbors(size_t hit_idx,
                                    std::vector<TDCHit>::const_iterator begin,
                                    std::vector<TDCHit>::const_iterator end,
                                    const DBSCANClusteringState& state) const;

  /**
   * @brief Expand cluster from a core point
   * @param core_idx Index of the core point
   * @param cluster_id Cluster ID to assign
   * @param begin Iterator to first hit
   * @param end Iterator to last hit
   * @param state DBSCAN clustering state
   * @param[out] cluster_labels Output cluster assignments
   */
  void expandCluster(size_t core_idx, int cluster_id,
                     std::vector<TDCHit>::const_iterator begin,
                     std::vector<TDCHit>::const_iterator end,
                     DBSCANClusteringState& state,
                     std::vector<int>& cluster_labels) const;

  /**
   * @brief Build spatial index for efficient neighbor queries
   * @param begin Iterator to first hit
   * @param end Iterator to last hit
   * @param state DBSCAN clustering state to populate
   */
  void buildSpatialIndex(std::vector<TDCHit>::const_iterator begin,
                         std::vector<TDCHit>::const_iterator end,
                         DBSCANClusteringState& state) const;

  /**
   * @brief Get spatial grid cell for a hit position
   * @param x X coordinate
   * @param y Y coordinate
   * @return Grid cell index
   */
  size_t getSpatialCell(double x, double y) const;

  /**
   * @brief Check if two hits are within epsilon distance
   * @param hit1 First hit
   * @param hit2 Second hit
   * @return True if within epsilon
   */
  bool withinEpsilon(const TDCHit& hit1, const TDCHit& hit2) const;

  /**
   * @brief Check if two hits are within temporal window
   * @param hit1 First hit
   * @param hit2 Second hit
   * @return True if within temporal correlation window
   */
  bool withinTemporalWindow(const TDCHit& hit1, const TDCHit& hit2) const;

 public:
  /**
   * @brief Default constructor with DBSCAN defaults
   */
  DBSCANHitClustering();

  /**
   * @brief Constructor with specific configuration
   */
  explicit DBSCANHitClustering(const HitClusteringConfig& config);

  // IHitClustering interface implementation
  void configure(const HitClusteringConfig& config) override;
  const HitClusteringConfig& getConfig() const override { return config_; }

  std::unique_ptr<IClusteringState> createState() const override;

  size_t cluster(std::vector<TDCHit>::const_iterator begin,
                 std::vector<TDCHit>::const_iterator end,
                 IClusteringState& state,
                 std::vector<int>& cluster_labels) const override;

  std::string getName() const override { return "dbscan"; }

  ClusteringStatistics getStatistics(const IClusteringState& state,
                                     size_t num_hits) const override;
};

/**
 * @brief State for DBSCAN clustering algorithm
 *
 * Contains spatial index and point classification data structures
 * needed for density-based clustering.
 */
class DBSCANClusteringState : public IClusteringState {
 public:
  // Point types in DBSCAN
  enum class PointType { UNDEFINED = 0, NOISE = 1, BORDER = 2, CORE = 3 };

  // Spatial index for efficient neighbor queries
  struct SpatialIndex {
    static constexpr size_t GRID_SIZE = 64;
    std::array<std::vector<size_t>, GRID_SIZE * GRID_SIZE> cells;

    void clear();
    void insert(size_t hit_idx, size_t cell);
    const std::vector<size_t>& getCell(size_t cell) const;
  };

  // Core data structures
  SpatialIndex spatial_index;
  std::vector<PointType> point_types;   // Classification of each point
  std::vector<bool> visited;            // Track visited points during expansion
  std::vector<size_t> neighbor_buffer;  // Reusable buffer for neighbor queries

  // Statistics
  size_t hits_processed = 0;
  size_t clusters_found = 0;
  size_t core_points = 0;
  size_t border_points = 0;
  size_t noise_points = 0;

  /**
   * @brief Constructor
   */
  DBSCANClusteringState();

  void reset() override;
  std::string getAlgorithmName() const override { return "dbscan"; }
};

}  // namespace tdcsophiread