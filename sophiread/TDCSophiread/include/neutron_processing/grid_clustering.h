// TDCSophiread Grid-Based Clustering
// Fast O(n) clustering using detector grid structure

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "neutron_processing/clustering_state.h"
#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

// Forward declaration
class GridClusteringState;

/**
 * @brief Grid-based clustering algorithm for temporal window processing
 *
 * This algorithm leverages the detector's natural grid structure to achieve
 * O(n) clustering performance. It divides the detector into a coarse grid
 * and clusters hits within each grid cell and its neighbors.
 *
 * Key features:
 * - O(n) time complexity
 * - Minimal memory overhead
 * - Natural fit for detector geometry
 * - Stateless design for parallel processing
 */
class GridHitClustering : public IHitClustering {
 public:
  GridHitClustering();
  explicit GridHitClustering(const HitClusteringConfig& config);
  ~GridHitClustering() override = default;

  // Configuration management
  void configure(const HitClusteringConfig& config) override;
  const HitClusteringConfig& getConfig() const override { return config_; }

  // State factory
  std::unique_ptr<IClusteringState> createState() const override;

  // Main clustering interface
  size_t cluster(std::vector<TDCHit>::const_iterator begin,
                 std::vector<TDCHit>::const_iterator end,
                 IClusteringState& state,
                 std::vector<int>& cluster_labels) const override;

  // Algorithm identification
  std::string getName() const override { return "grid"; }

  // Statistics
  ClusteringStatistics getStatistics(const IClusteringState& state,
                                     size_t num_hits) const override;

 private:
  HitClusteringConfig config_;

  // Grid dimensions based on config
  size_t getGridCols() const;
  size_t getGridRows() const;

  // Helper functions
  size_t getGridCell(double x, double y, size_t cols, size_t rows) const;
  void processGridCell(size_t cell_idx,
                       std::vector<TDCHit>::const_iterator begin,
                       GridClusteringState& state,
                       std::vector<int>& cluster_labels,
                       int& next_cluster_id) const;

  bool areHitsConnected(const TDCHit& hit1, const TDCHit& hit2) const;
  void mergeClusterLabels(std::vector<int>& cluster_labels, int old_label,
                          int new_label) const;
};

/**
 * @brief State container for grid-based clustering
 *
 * Maintains grid structure and clustering state between batches
 */
class GridClusteringState : public IClusteringState {
 public:
  // Grid cell structure
  struct GridCell {
    std::vector<size_t> hit_indices;

    void clear() { hit_indices.clear(); }
  };

  GridClusteringState();
  ~GridClusteringState() override = default;

  // State management
  void reset() override;
  std::string getAlgorithmName() const override { return "grid"; }

  // Grid management
  void initializeGrid(size_t cols, size_t rows);
  void insertHit(size_t hit_idx, size_t cell_idx);
  const GridCell& getCell(size_t cell_idx) const;
  size_t getNumCells() const { return grid_cells.size(); }

  // Statistics
  size_t hits_processed = 0;
  size_t clusters_found = 0;
  size_t cells_with_hits = 0;
  size_t max_hits_per_cell = 0;

 private:
  std::vector<GridCell> grid_cells;
  size_t grid_cols = 0;
  size_t grid_rows = 0;
};

}  // namespace tdcsophiread