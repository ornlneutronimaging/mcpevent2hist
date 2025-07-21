// TDCSophiread Grid Clustering Unit Tests
// Test suite for the grid-based clustering algorithm

#include <gtest/gtest.h>

#include <vector>

#include "neutron_processing/grid_clustering.h"
#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

class GridClusteringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create clustering algorithm with default config
    config_.algorithm = "grid";
    config_.grid.grid_cols = 16;  // Smaller grid for easier testing
    config_.grid.grid_rows = 16;
    config_.grid.connection_distance = 4.0;
    config_.grid.neutron_correlation_window = 75.0;
    config_.grid.merge_adjacent_cells = true;

    clusterer_ = std::make_unique<GridHitClustering>(config_);
  }

  // Helper function to create test hits
  TDCHit createHit(uint16_t x, uint16_t y, uint32_t tof, uint16_t tot = 100) {
    return TDCHit(tof, x, y, 0, tot, 0);
  }

  HitClusteringConfig config_;
  std::unique_ptr<GridHitClustering> clusterer_;
};

// Test default construction
TEST_F(GridClusteringTest, DefaultConstruction) {
  GridHitClustering default_clusterer;
  EXPECT_EQ("grid", default_clusterer.getName());

  auto config = default_clusterer.getConfig();
  EXPECT_EQ("grid", config.algorithm);
  EXPECT_EQ(32, config.grid.grid_cols);
  EXPECT_EQ(32, config.grid.grid_rows);
  EXPECT_EQ(4.0, config.grid.connection_distance);
  EXPECT_EQ(75.0, config.grid.neutron_correlation_window);
  EXPECT_TRUE(config.grid.merge_adjacent_cells);
}

// Test state creation
TEST_F(GridClusteringTest, StateCreation) {
  auto state = clusterer_->createState();
  ASSERT_NE(nullptr, state);
  EXPECT_EQ("grid", state->getAlgorithmName());

  // Cast to specific state type
  auto* grid_state = dynamic_cast<GridClusteringState*>(state.get());
  ASSERT_NE(nullptr, grid_state);

  // Check initial state
  EXPECT_EQ(0, grid_state->hits_processed);
  EXPECT_EQ(0, grid_state->clusters_found);
  EXPECT_EQ(0, grid_state->cells_with_hits);
  EXPECT_EQ(0, grid_state->max_hits_per_cell);
}

// Test empty clustering
TEST_F(GridClusteringTest, EmptyHits) {
  std::vector<TDCHit> hits;
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(0, num_clusters);
  EXPECT_TRUE(labels.empty());
}

// Test single hit
TEST_F(GridClusteringTest, SingleHit) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000)};
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(1, num_clusters);
  ASSERT_EQ(1, labels.size());
  EXPECT_EQ(0, labels[0]);  // Single hit forms cluster 0

  auto* grid_state = dynamic_cast<GridClusteringState*>(state.get());
  EXPECT_EQ(1, grid_state->cells_with_hits);
  EXPECT_EQ(1, grid_state->max_hits_per_cell);
}

// Test hits in same grid cell
TEST_F(GridClusteringTest, HitsInSameCell) {
  // With 16x16 grid and 512x512 detector, each cell is 32x32 pixels
  // Hits at (100,100) and (120,120) should be in same cell
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(102, 102, 1001),  // Within connection distance (dist=2.83)
      createHit(120, 120, 1002),  // Same cell but far
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(2, num_clusters);  // Two clusters in same cell
  ASSERT_EQ(3, labels.size());
  EXPECT_EQ(labels[0], labels[1]);  // First two connected
  EXPECT_NE(labels[0], labels[2]);  // Third is separate cluster
}

// Test hits in adjacent cells
TEST_F(GridClusteringTest, HitsInAdjacentCells) {
  // Cell boundary at x=128 (cell size = 512/16 = 32)
  std::vector<TDCHit> hits = {
      createHit(127, 100, 1000),  // Cell (3, 3)
      createHit(129, 100, 1001),  // Cell (4, 3) - adjacent
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  // With merge_adjacent_cells=true and connection_distance=4.0
  // These should merge into one cluster
  EXPECT_EQ(1, num_clusters);
  ASSERT_EQ(2, labels.size());
  EXPECT_EQ(labels[0], labels[1]);
}

// Test without cell merging
TEST_F(GridClusteringTest, NoMergingAcrossCells) {
  config_.grid.merge_adjacent_cells = false;
  clusterer_->configure(config_);

  // Same hits as above test
  std::vector<TDCHit> hits = {
      createHit(127, 100, 1000),  // Cell (3, 3)
      createHit(129, 100, 1001),  // Cell (4, 3) - adjacent
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  // Without merging, each cell gets separate clusters
  EXPECT_EQ(2, num_clusters);
  ASSERT_EQ(2, labels.size());
  EXPECT_NE(labels[0], labels[1]);
}

// Test temporal window constraint
TEST_F(GridClusteringTest, TemporalWindowConstraint) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(102, 102, 1001),  // Within spatial and temporal
      createHit(104, 104,
                1010),  // Within spatial but outside temporal (250ns > 75ns)
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(2, num_clusters);
  ASSERT_EQ(3, labels.size());
  EXPECT_EQ(labels[0], labels[1]);  // First two connected
  EXPECT_NE(labels[0], labels[2]);  // Third is separate due to time
}

// Test grid structure efficiency
TEST_F(GridClusteringTest, GridStructureEfficiency) {
  // Create hits spread across multiple cells
  std::vector<TDCHit> hits;

  // Add clusters in different grid cells
  for (int cell = 0; cell < 4; ++cell) {
    int base_x = cell * 100;
    int base_y = cell * 100;

    // Add 5 hits per cluster
    for (int i = 0; i < 5; ++i) {
      hits.push_back(createHit(base_x + i, base_y + i, 1000 + i));
    }
  }

  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(4, num_clusters);  // 4 separate clusters

  auto* grid_state = dynamic_cast<GridClusteringState*>(state.get());
  EXPECT_EQ(4, grid_state->cells_with_hits);
  EXPECT_EQ(5, grid_state->max_hits_per_cell);
}

// Test dense cluster within grid cell
TEST_F(GridClusteringTest, DenseClusterInCell) {
  // Create dense cluster that fits within one grid cell
  std::vector<TDCHit> hits;
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      if ((i + j) % 3 == 0) {  // Sparse pattern
        hits.push_back(createHit(200 + i, 200 + j, 1000));
      }
    }
  }

  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  // All hits should be in same cell and form one cluster
  // (connection_distance=4.0 connects the sparse pattern)
  EXPECT_EQ(1, num_clusters);

  auto* grid_state = dynamic_cast<GridClusteringState*>(state.get());
  EXPECT_EQ(1, grid_state->cells_with_hits);
}

// Test edge case: hits at detector boundaries
TEST_F(GridClusteringTest, DetectorBoundaryHits) {
  std::vector<TDCHit> hits = {
      createHit(0, 0, 1000),      // Top-left corner
      createHit(511, 0, 1001),    // Top-right corner
      createHit(0, 511, 1002),    // Bottom-left corner
      createHit(511, 511, 1003),  // Bottom-right corner
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(4, num_clusters);  // 4 separate clusters at corners

  // Verify all labels are valid
  for (int label : labels) {
    EXPECT_GE(label, 0);
    EXPECT_LT(label, 4);
  }
}

// Test realistic neutron event pattern
TEST_F(GridClusteringTest, RealisticNeutronPattern) {
  // Configure for realistic neutron detection
  config_.grid.grid_cols = 32;  // Finer grid
  config_.grid.grid_rows = 32;
  config_.grid.connection_distance = 3.0;
  clusterer_->configure(config_);

  // Simulate two neutron events
  std::vector<TDCHit> hits = {
      // First neutron event - compact cluster
      createHit(256, 256, 10000, 150),
      createHit(257, 256, 10001, 140),
      createHit(256, 257, 10001, 145),
      createHit(258, 257, 10002, 130),
      // Second neutron event
      createHit(300, 300, 10100, 160),
      createHit(301, 301, 10101, 155),
      createHit(300, 301, 10101, 150),
      // Background/noise hits
      createHit(150, 150, 10000, 50),
      createHit(400, 400, 10200, 45),
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(4, num_clusters);  // Two neutron clusters + 2 noise

  // Get statistics
  auto stats = clusterer_->getStatistics(*state, hits.size());
  EXPECT_EQ(4, stats.total_clusters);
  EXPECT_GT(stats.mean_cluster_size, 0.0);
}

// Test state reset
TEST_F(GridClusteringTest, StateReset) {
  auto state = clusterer_->createState();
  auto* grid_state = dynamic_cast<GridClusteringState*>(state.get());

  // First clustering
  std::vector<TDCHit> hits1 = {
      createHit(100, 100, 1000),
      createHit(101, 101, 1001),
  };
  std::vector<int> labels1(hits1.size(), -1);
  clusterer_->cluster(hits1.begin(), hits1.end(), *state, labels1);

  // Verify state has data
  EXPECT_GT(grid_state->hits_processed, 0);
  EXPECT_GT(grid_state->clusters_found, 0);
  EXPECT_GT(grid_state->cells_with_hits, 0);

  // Reset state
  state->reset();

  // Verify clean state
  EXPECT_EQ(0, grid_state->hits_processed);
  EXPECT_EQ(0, grid_state->clusters_found);
  EXPECT_EQ(0, grid_state->cells_with_hits);
  EXPECT_EQ(0, grid_state->max_hits_per_cell);

  // Second clustering should work correctly
  std::vector<TDCHit> hits2 = {createHit(200, 200, 2000)};
  std::vector<int> labels2(hits2.size(), -1);
  size_t num_clusters =
      clusterer_->cluster(hits2.begin(), hits2.end(), *state, labels2);

  EXPECT_EQ(1, num_clusters);
  EXPECT_EQ(1, grid_state->cells_with_hits);
}

}  // namespace tdcsophiread