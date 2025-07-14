// TDCSophiread Graph Clustering Unit Tests
// Test suite for the new graph-based clustering algorithm

#include <gtest/gtest.h>

#include <vector>

#include "neutron_processing/graph_clustering.h"
#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

class GraphClusteringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create clustering algorithm with default config
    config_.algorithm = "graph";
    config_.graph.radius = 5.0;
    config_.graph.min_cluster_size = 1;
    config_.abs.neutron_correlation_window = 3000;  // 75ns in 25ns units

    clusterer_ = std::make_unique<GraphHitClustering>(config_);
  }

  // Helper function to create test hits
  TDCHit createHit(uint16_t x, uint16_t y, uint32_t tof, uint16_t tot = 100) {
    return TDCHit(tof, x, y, 0, tot, 0);
  }

  HitClusteringConfig config_;
  std::unique_ptr<GraphHitClustering> clusterer_;
};

// Test default construction
TEST_F(GraphClusteringTest, DefaultConstruction) {
  GraphHitClustering default_clusterer;
  EXPECT_EQ("graph", default_clusterer.getName());

  auto config = default_clusterer.getConfig();
  EXPECT_EQ("graph", config.algorithm);
  EXPECT_EQ(5.0, config.graph.radius);
  EXPECT_EQ(1, config.graph.min_cluster_size);
}

// Test state creation
TEST_F(GraphClusteringTest, StateCreation) {
  auto state = clusterer_->createState();
  ASSERT_NE(nullptr, state);
  EXPECT_EQ("graph", state->getAlgorithmName());

  // Cast to specific state type
  auto* graph_state = dynamic_cast<GraphClusteringState*>(state.get());
  ASSERT_NE(nullptr, graph_state);

  // Check initial state
  EXPECT_EQ(0, graph_state->edges_created);
  EXPECT_EQ(0, graph_state->hits_processed);
  EXPECT_EQ(0, graph_state->clusters_found);
}

// Test empty clustering
TEST_F(GraphClusteringTest, EmptyHits) {
  std::vector<TDCHit> hits;
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(0, num_clusters);
  EXPECT_TRUE(labels.empty());
}

// Test single hit
TEST_F(GraphClusteringTest, SingleHit) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000)};
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(1, num_clusters);
  ASSERT_EQ(1, labels.size());
  EXPECT_EQ(0, labels[0]);  // First cluster gets ID 0
}

// Test two hits within radius
TEST_F(GraphClusteringTest, TwoHitsWithinRadius) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(102, 103, 1010)  // Distance ~3.6, within radius 5
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(1, num_clusters);
  ASSERT_EQ(2, labels.size());
  EXPECT_EQ(labels[0], labels[1]);  // Same cluster
}

// Test two hits outside radius
TEST_F(GraphClusteringTest, TwoHitsOutsideRadius) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(106, 106, 1010)  // Distance ~8.5, outside radius 5
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(2, num_clusters);
  ASSERT_EQ(2, labels.size());
  EXPECT_NE(labels[0], labels[1]);  // Different clusters
}

// Test temporal window constraint
TEST_F(GraphClusteringTest, TemporalWindowConstraint) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(101, 101, 4500)  // Within radius but 3500 units apart (>3000)
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(2, num_clusters);
  ASSERT_EQ(2, labels.size());
  EXPECT_NE(labels[0], labels[1]);  // Different clusters due to temporal gap
}

// Test chain of hits (graph connectivity)
TEST_F(GraphClusteringTest, ChainOfHits) {
  // Create chain where each hit connects to next but not all to all
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000), createHit(104, 100, 1010),  // 4 pixels away
      createHit(108, 100, 1020),  // 4 pixels from previous, 8 from first
      createHit(112, 100, 1030)   // 4 pixels from previous, 12 from first
  };
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(1, num_clusters);
  ASSERT_EQ(4, labels.size());
  // All should be in same cluster due to graph connectivity
  EXPECT_EQ(labels[0], labels[1]);
  EXPECT_EQ(labels[1], labels[2]);
  EXPECT_EQ(labels[2], labels[3]);
}

// Test minimum cluster size filtering
TEST_F(GraphClusteringTest, MinimumClusterSize) {
  // Configure to require at least 3 hits per cluster
  config_.graph.min_cluster_size = 3;
  clusterer_->configure(config_);

  std::vector<TDCHit> hits = {
      // Small cluster (2 hits) - should be filtered out
      createHit(100, 100, 1000), createHit(101, 101, 1010),
      // Large cluster (3 hits) - should be kept
      createHit(200, 200, 1000), createHit(201, 201, 1010),
      createHit(202, 202, 1020),
      // Single hit - should be filtered out
      createHit(300, 300, 1000)};
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(1, num_clusters);  // Only the large cluster survives
  ASSERT_EQ(6, labels.size());

  // Small cluster should be marked as noise
  EXPECT_EQ(-1, labels[0]);
  EXPECT_EQ(-1, labels[1]);

  // Large cluster should have same ID
  EXPECT_GE(labels[2], 0);
  EXPECT_EQ(labels[2], labels[3]);
  EXPECT_EQ(labels[3], labels[4]);

  // Single hit should be noise
  EXPECT_EQ(-1, labels[5]);
}

// Test with realistic neutron event pattern
TEST_F(GraphClusteringTest, RealisticNeutronEvent) {
  // Simulate neutron event with characteristic spatial spread
  std::vector<TDCHit> hits = {
      // Central hits
      createHit(250, 250, 5000, 150), createHit(251, 249, 5002, 140),
      createHit(249, 251, 5001, 145),
      // Peripheral hits
      createHit(253, 248, 5003, 80), createHit(247, 252, 5004, 75),
      // Noise hit far away
      createHit(280, 280, 5010, 50)};
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(2, num_clusters);  // Main cluster + noise
  ASSERT_EQ(6, labels.size());

  // First 5 hits should be in same cluster
  for (int i = 1; i < 5; ++i) {
    EXPECT_EQ(labels[0], labels[i]);
  }

  // Last hit should be separate
  EXPECT_NE(labels[0], labels[5]);
}

// Test state reset
TEST_F(GraphClusteringTest, StateReset) {
  auto state = clusterer_->createState();
  auto* graph_state = static_cast<GraphClusteringState*>(state.get());

  // First clustering
  std::vector<TDCHit> hits1 = {createHit(100, 100, 1000),
                               createHit(101, 101, 1010)};
  std::vector<int> labels1(hits1.size(), -1);
  clusterer_->cluster(hits1.begin(), hits1.end(), *state, labels1);

  EXPECT_GT(graph_state->edges_created, 0);
  EXPECT_EQ(2, graph_state->hits_processed);

  // Reset state
  state->reset();

  EXPECT_EQ(0, graph_state->edges_created);
  EXPECT_EQ(0, graph_state->hits_processed);
  EXPECT_EQ(0, graph_state->clusters_found);

  // Second clustering with same state
  std::vector<TDCHit> hits2 = {createHit(200, 200, 2000),
                               createHit(201, 201, 2010),
                               createHit(202, 202, 2020)};
  std::vector<int> labels2(hits2.size(), -1);
  size_t num_clusters =
      clusterer_->cluster(hits2.begin(), hits2.end(), *state, labels2);

  EXPECT_EQ(1, num_clusters);
  EXPECT_EQ(3, graph_state->hits_processed);
}

// Test clustering statistics
TEST_F(GraphClusteringTest, ClusteringStatistics) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000), createHit(101, 101, 1010),
      createHit(200, 200, 2000), createHit(201, 201, 2010),
      createHit(202, 202, 2020)};
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(2, num_clusters);

  auto stats = clusterer_->getStatistics(*state, hits.size());
  EXPECT_EQ(2, stats.total_clusters);
  EXPECT_EQ(5, stats.total_hits_processed);
  EXPECT_EQ(0, stats.unclustered_hits);  // Graph clustering has no unclustered
  EXPECT_DOUBLE_EQ(2.5, stats.mean_cluster_size);  // 5 hits / 2 clusters
}

// Test edge case: all hits at same position
TEST_F(GraphClusteringTest, ColocatedHits) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000), createHit(100, 100, 1001),
      createHit(100, 100, 1002), createHit(100, 100, 1003)};
  std::vector<int> labels(hits.size(), -1);

  auto state = clusterer_->createState();
  size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_EQ(1, num_clusters);
  ASSERT_EQ(4, labels.size());
  // All should be in same cluster
  for (int i = 1; i < 4; ++i) {
    EXPECT_EQ(labels[0], labels[i]);
  }
}

}  // namespace tdcsophiread