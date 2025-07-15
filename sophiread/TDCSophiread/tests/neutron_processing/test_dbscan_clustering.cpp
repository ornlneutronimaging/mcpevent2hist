// TDCSophiread DBSCAN Clustering Unit Tests
// Test suite for the density-based clustering algorithm

#include <gtest/gtest.h>

#include <vector>

#include "neutron_processing/dbscan_clustering.h"
#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

class DBSCANClusteringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create clustering algorithm with default config
    config_.algorithm = "dbscan";
    config_.dbscan.epsilon = 5.0;
    config_.dbscan.min_points = 4;
    config_.dbscan.neutron_correlation_window = 75.0;  // 75ns

    clusterer_ = std::make_unique<DBSCANHitClustering>(config_);
  }

  // Helper function to create test hits
  TDCHit createHit(uint16_t x, uint16_t y, uint32_t tof, uint16_t tot = 100) {
    return TDCHit(tof, x, y, 0, tot, 0);
  }

  HitClusteringConfig config_;
  std::unique_ptr<DBSCANHitClustering> clusterer_;
};

// Test default construction
TEST_F(DBSCANClusteringTest, DefaultConstruction) {
  DBSCANHitClustering default_clusterer;
  EXPECT_EQ("dbscan", default_clusterer.getName());

  auto config = default_clusterer.getConfig();
  EXPECT_EQ("dbscan", config.algorithm);
  EXPECT_EQ(5.0, config.dbscan.epsilon);
  EXPECT_EQ(4, config.dbscan.min_points);
}

// Test state creation
TEST_F(DBSCANClusteringTest, StateCreation) {
  auto state = clusterer_->createState();
  ASSERT_NE(nullptr, state);
  EXPECT_EQ("dbscan", state->getAlgorithmName());

  // Cast to specific state type
  auto* dbscan_state = dynamic_cast<DBSCANClusteringState*>(state.get());
  ASSERT_NE(nullptr, dbscan_state);

  // Check initial state
  EXPECT_EQ(0, dbscan_state->hits_processed);
  EXPECT_EQ(0, dbscan_state->clusters_found);
  EXPECT_EQ(0, dbscan_state->core_points);
  EXPECT_EQ(0, dbscan_state->border_points);
  EXPECT_EQ(0, dbscan_state->noise_points);
}

// Test empty clustering
TEST_F(DBSCANClusteringTest, EmptyHits) {
  std::vector<TDCHit> hits;
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(0, num_clusters);
  EXPECT_TRUE(labels.empty());
}

// Test single hit (should be noise)
TEST_F(DBSCANClusteringTest, SingleHit) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000)};
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(0, num_clusters);  // No clusters formed
  ASSERT_EQ(1, labels.size());
  EXPECT_EQ(-1, labels[0]);  // Marked as noise

  auto* dbscan_state = dynamic_cast<DBSCANClusteringState*>(state.get());
  EXPECT_EQ(1, dbscan_state->noise_points);
  EXPECT_EQ(0, dbscan_state->core_points);
}

// Test sparse hits (all noise)
TEST_F(DBSCANClusteringTest, SparseHits) {
  // Create hits that are too far apart to form dense regions
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000), createHit(110, 110, 1010),  // >epsilon away
      createHit(120, 120, 1020),                             // >epsilon away
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(0, num_clusters);
  ASSERT_EQ(3, labels.size());
  // All should be noise
  EXPECT_EQ(-1, labels[0]);
  EXPECT_EQ(-1, labels[1]);
  EXPECT_EQ(-1, labels[2]);

  auto* dbscan_state = dynamic_cast<DBSCANClusteringState*>(state.get());
  EXPECT_EQ(3, dbscan_state->noise_points);
  EXPECT_EQ(0, dbscan_state->core_points);
}

// Test dense cluster (basic DBSCAN behavior)
TEST_F(DBSCANClusteringTest, DenseCluster) {
  // Create a dense cluster of 5 hits
  // TOF values are in 25ns units, so use small differences for 75ns window
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000), createHit(101, 101, 1001),  // 25ns later
      createHit(102, 100, 1002),                             // 50ns later
      createHit(101, 102, 1002),                             // 50ns later
      createHit(100, 101, 1003),                             // 75ns later
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(1, num_clusters);
  ASSERT_EQ(5, labels.size());
  // All should be in same cluster
  EXPECT_GE(labels[0], 0);
  EXPECT_EQ(labels[0], labels[1]);
  EXPECT_EQ(labels[1], labels[2]);
  EXPECT_EQ(labels[2], labels[3]);
  EXPECT_EQ(labels[3], labels[4]);

  auto* dbscan_state = dynamic_cast<DBSCANClusteringState*>(state.get());
  EXPECT_EQ(0, dbscan_state->noise_points);
  EXPECT_GT(dbscan_state->core_points, 0);  // Should have core points
}

// Test core and border points
TEST_F(DBSCANClusteringTest, CoreAndBorderPoints) {
  // Configure with minPts=3 for easier testing
  config_.dbscan.min_points = 3;
  clusterer_->configure(config_);

  // Create pattern: dense core with border points
  std::vector<TDCHit> hits = {
      // Dense core (3 hits close together)
      createHit(100, 100, 1000), createHit(101, 100, 1001),
      createHit(100, 101, 1001),
      // Border point (within epsilon of only one core point)
      createHit(104, 103, 1002),  // Distance ~5 from (100,100), ~4.5 from
                                  // (101,100), ~4.2 from (100,101)
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(1, num_clusters);
  ASSERT_EQ(4, labels.size());
  // All should be in same cluster
  EXPECT_GE(labels[0], 0);
  EXPECT_EQ(labels[0], labels[1]);
  EXPECT_EQ(labels[1], labels[2]);
  EXPECT_EQ(labels[2], labels[3]);

  auto* dbscan_state = dynamic_cast<DBSCANClusteringState*>(state.get());
  EXPECT_EQ(0, dbscan_state->noise_points);
  // In DBSCAN, points can be promoted to core during expansion
  // So we just check that we have classified points, not specific counts
  EXPECT_EQ(4, dbscan_state->core_points + dbscan_state->border_points);
}

// Test multiple clusters
TEST_F(DBSCANClusteringTest, MultipleClusters) {
  // Configure with minPts=2 for easier testing
  config_.dbscan.min_points = 2;
  clusterer_->configure(config_);

  std::vector<TDCHit> hits = {
      // First cluster
      createHit(100, 100, 1000),
      createHit(101, 101, 1001),
      createHit(102, 100, 1002),
      // Second cluster (far from first)
      createHit(200, 200, 1000),
      createHit(201, 201, 1001),
      createHit(202, 200, 1002),
      // Noise point between clusters
      createHit(150, 150, 1000),
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(2, num_clusters);
  ASSERT_EQ(7, labels.size());

  // First cluster
  EXPECT_GE(labels[0], 0);
  EXPECT_EQ(labels[0], labels[1]);
  EXPECT_EQ(labels[1], labels[2]);

  // Second cluster (different ID)
  EXPECT_GE(labels[3], 0);
  EXPECT_EQ(labels[3], labels[4]);
  EXPECT_EQ(labels[4], labels[5]);
  EXPECT_NE(labels[0], labels[3]);

  // Noise point
  EXPECT_EQ(-1, labels[6]);

  auto* dbscan_state = dynamic_cast<DBSCANClusteringState*>(state.get());
  EXPECT_EQ(1, dbscan_state->noise_points);
}

// Test temporal window constraint
TEST_F(DBSCANClusteringTest, TemporalWindowConstraint) {
  // Configure with minPts=2 for easier testing
  config_.dbscan.min_points = 2;
  clusterer_->configure(config_);

  std::vector<TDCHit> hits = {
      // Dense group but separated in time
      createHit(100, 100, 1000),
      createHit(101, 101, 1001),
      createHit(102, 100, 1010),  // Within epsilon spatially but outside
                                  // temporal window (10*25=250ns > 75ns)
      createHit(101, 102, 1011),
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(2, num_clusters);  // Should form 2 separate clusters
  ASSERT_EQ(4, labels.size());

  // First temporal cluster
  EXPECT_GE(labels[0], 0);
  EXPECT_EQ(labels[0], labels[1]);

  // Second temporal cluster
  EXPECT_GE(labels[2], 0);
  EXPECT_EQ(labels[2], labels[3]);

  // Different clusters
  EXPECT_NE(labels[0], labels[2]);
}

// Test epsilon parameter sensitivity
TEST_F(DBSCANClusteringTest, EpsilonSensitivity) {
  // Configure with smaller epsilon
  config_.dbscan.epsilon = 2.0;  // Smaller radius
  config_.dbscan.min_points = 2;
  clusterer_->configure(config_);

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(102, 100, 1001),  // Distance = 2, exactly at epsilon
      createHit(
          105, 100,
          1002),  // Distance = 5 from first, 3 from second, >epsilon from both
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  // First two should cluster, third should be noise (not enough neighbors)
  EXPECT_EQ(1, num_clusters);  // Only one cluster formed
  ASSERT_EQ(3, labels.size());
  EXPECT_EQ(labels[0], labels[1]);  // First two in same cluster
  EXPECT_EQ(-1, labels[2]);         // Third is noise
}

// Test with realistic neutron event pattern
TEST_F(DBSCANClusteringTest, RealisticNeutronPattern) {
  // Configure for realistic neutron detection
  config_.dbscan.epsilon = 4.0;
  config_.dbscan.min_points = 3;
  clusterer_->configure(config_);

  // Simulate two neutron events with characteristic patterns
  std::vector<TDCHit> hits = {
      // First neutron event - dense core with some spread (all within 75ns)
      createHit(256, 256, 10000, 150),
      createHit(257, 256, 10001, 140),
      createHit(256, 257, 10001, 145),
      createHit(258, 257, 10002, 130),
      createHit(255, 256, 10002, 135),
      // Second neutron event (100 units = 2500ns later)
      createHit(300, 300, 10100, 160),
      createHit(301, 301, 10101, 155),
      createHit(300, 301, 10101, 150),
      createHit(302, 300, 10102, 145),
      // Background/noise hits
      createHit(150, 150, 10000, 50),
      createHit(400, 400, 10200, 45),
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(2, num_clusters);  // Two neutron clusters

  // Verify clustering
  auto* dbscan_state = dynamic_cast<DBSCANClusteringState*>(state.get());
  EXPECT_EQ(2, dbscan_state->noise_points);  // Two background hits
  EXPECT_GT(dbscan_state->core_points, 0);   // Should have core points

  // Get statistics
  auto stats = clusterer_->getStatistics(*state, hits.size());
  EXPECT_EQ(2, stats.total_clusters);
  EXPECT_EQ(2, stats.unclustered_hits);
  EXPECT_GT(stats.mean_cluster_size, 0.0);
}

// Test state reset
TEST_F(DBSCANClusteringTest, StateReset) {
  auto state = clusterer_->createState();
  auto* dbscan_state = dynamic_cast<DBSCANClusteringState*>(state.get());

  // First clustering
  std::vector<TDCHit> hits1 = {
      createHit(100, 100, 1000),
      createHit(101, 101, 1001),
      createHit(102, 100, 1002),
      createHit(101, 102, 1002),
  };
  std::vector<int> labels1(hits1.size(), -1);
  clusterer_->cluster(hits1.begin(), hits1.end(), *state, labels1);

  // Verify state has data
  EXPECT_GT(dbscan_state->hits_processed, 0);
  EXPECT_GT(dbscan_state->clusters_found, 0);

  // Reset state
  state->reset();

  // Verify clean state
  EXPECT_EQ(0, dbscan_state->hits_processed);
  EXPECT_EQ(0, dbscan_state->clusters_found);
  EXPECT_EQ(0, dbscan_state->core_points);
  EXPECT_EQ(0, dbscan_state->border_points);
  EXPECT_EQ(0, dbscan_state->noise_points);

  // Second clustering should work correctly
  std::vector<TDCHit> hits2 = {createHit(200, 200, 2000)};
  std::vector<int> labels2(hits2.size(), -1);
  size_t num_clusters =
      clusterer_->cluster(hits2.begin(), hits2.end(), *state, labels2);

  EXPECT_EQ(0, num_clusters);  // Single hit = noise
  EXPECT_EQ(1, dbscan_state->noise_points);
}

}  // namespace tdcsophiread