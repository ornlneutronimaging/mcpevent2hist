// TDCSophiread Graph Clustering
// Graph-based clustering implementation for temporal window processing
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "neutron_processing/clustering_state.h"
#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

// Forward declaration
class GraphClusteringState;

/**
 * @brief Graph-based hit clustering implementation (stateless)
 *
 * Implements graph-based spatial-temporal clustering where hits are
 * connected if they are within a spatial radius and temporal window.
 * Uses Union-Find algorithm for efficient connected component detection.
 *
 * This is a new implementation designed specifically for the temporal
 * window architecture with improved performance and thread safety.
 */
class GraphHitClustering : public IHitClustering {
 private:
  // Configuration (immutable after construction)
  HitClusteringConfig config_;

  // Spatial grid parameters for efficient neighbor search
  static constexpr size_t SPATIAL_GRID_SIZE = 64;  // Finer grid for graph
  static constexpr double DEFAULT_RADIUS = 5.0;    // Default spatial radius

  /**
   * @brief Build graph edges between hits within spatial/temporal constraints
   * @param begin Iterator to first hit
   * @param end Iterator to last hit
   * @param state Graph clustering state to populate
   */
  void buildGraph(std::vector<TDCHit>::const_iterator begin,
                  std::vector<TDCHit>::const_iterator end,
                  GraphClusteringState& state) const;

  /**
   * @brief Find connected components using Union-Find
   * @param state Graph clustering state with built edges
   * @param[out] cluster_labels Output cluster assignments
   * @return Number of clusters found
   */
  size_t findConnectedComponents(GraphClusteringState& state,
                                 std::vector<int>& cluster_labels) const;

  /**
   * @brief Get spatial grid cell for a hit position
   * @param x X coordinate
   * @param y Y coordinate
   * @return Grid cell index
   */
  size_t getSpatialCell(double x, double y) const;

  /**
   * @brief Check if two hits are within spatial radius
   * @param hit1 First hit
   * @param hit2 Second hit
   * @return True if within radius
   */
  bool withinRadius(const TDCHit& hit1, const TDCHit& hit2) const;

  /**
   * @brief Check if two hits are within temporal window
   * @param hit1 First hit
   * @param hit2 Second hit
   * @return True if within temporal correlation window
   */
  bool withinTemporalWindow(const TDCHit& hit1, const TDCHit& hit2) const;

 public:
  /**
   * @brief Default constructor with graph defaults
   */
  GraphHitClustering();

  /**
   * @brief Constructor with specific configuration
   */
  explicit GraphHitClustering(const HitClusteringConfig& config);

  // IHitClustering interface implementation
  void configure(const HitClusteringConfig& config) override;
  const HitClusteringConfig& getConfig() const override { return config_; }

  std::unique_ptr<IClusteringState> createState() const override;

  size_t cluster(std::vector<TDCHit>::const_iterator begin,
                 std::vector<TDCHit>::const_iterator end,
                 IClusteringState& state,
                 std::vector<int>& cluster_labels) const override;

  std::string getName() const override { return "graph"; }

  ClusteringStatistics getStatistics(const IClusteringState& state,
                                     size_t num_hits) const override;
};

/**
 * @brief State for graph-based clustering algorithm
 *
 * Contains Union-Find data structure and spatial index for
 * efficient graph construction and connected component detection.
 */
class GraphClusteringState : public IClusteringState {
 public:
  // Union-Find data structure
  struct UnionFind {
    std::vector<int> parent;  // Parent of each node
    std::vector<int> rank;    // Rank for union by rank

    void init(size_t n);
    int find(int x);
    void unite(int x, int y);
    void reset();
  };

  // Spatial index for efficient neighbor queries
  struct SpatialIndex {
    static constexpr size_t GRID_SIZE = 64;
    std::array<std::vector<size_t>, GRID_SIZE * GRID_SIZE> cells;

    void clear();
    void insert(size_t hit_idx, size_t cell);
    const std::vector<size_t>& getCell(size_t cell) const;
  };

  // Core data structures
  UnionFind union_find;
  SpatialIndex spatial_index;

  // Statistics
  size_t edges_created = 0;
  size_t hits_processed = 0;
  size_t clusters_found = 0;

  /**
   * @brief Constructor
   */
  GraphClusteringState();

  void reset() override;
  std::string getAlgorithmName() const override { return "graph"; }
};

}  // namespace tdcsophiread