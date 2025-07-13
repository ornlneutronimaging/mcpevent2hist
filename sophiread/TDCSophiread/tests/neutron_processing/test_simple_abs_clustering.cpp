// TDCSophiread Simple ABS Clustering Algorithm Tests
// Tests for correctness of box distance clustering and bucket management

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "neutron_processing/neutron_config.h"
#include "neutron_processing/simple_abs_clustering.h"
#include "tdc_hit.h"

namespace tdcsophiread {

/**
 * @brief Test fixture for Simple ABS clustering algorithm
 */
class SimpleABSClusteringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test configuration based on VENUS defaults
    config_ = HitClusteringConfig();
    config_.algorithm = "simple_abs";
    config_.abs.radius = 5.0;                       // 5-pixel radius
    config_.abs.min_cluster_size = 1;               // Any cluster valid
    config_.abs.neutron_correlation_window = 75.0;  // 75ns window
    config_.abs.scan_interval = 100;                // Scan every 100 hits

    // Create clustering instance
    clusterer_ = std::make_unique<SimpleABSClustering>(config_);
  }

  TDCHit createHit(uint16_t x, uint16_t y, uint32_t tof, uint16_t tot = 100,
                   uint8_t chip_id = 0) {
    TDCHit hit;
    hit.x = x;
    hit.y = y;
    hit.tof = tof;
    hit.tot = tot;
    hit.chip_id = chip_id;
    hit.timestamp = tof;  // Simple case
    return hit;
  }

  HitClusteringConfig config_;
  std::unique_ptr<SimpleABSClustering> clusterer_;
};

// Test 1: Empty input handling
TEST_F(SimpleABSClusteringTest, EmptyInput) {
  std::vector<TDCHit> empty_hits;

  size_t num_clusters =
      clusterer_->cluster(empty_hits.begin(), empty_hits.end());

  EXPECT_EQ(num_clusters, 0);
  EXPECT_EQ(clusterer_->getClusterLabels().size(), 0);
  EXPECT_EQ(clusterer_->getLastHitCount(), 0);
}

// Test 2: Single hit processing
TEST_F(SimpleABSClusteringTest, SingleHit) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000)};

  [[maybe_unused]] size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end());
  const auto& labels = clusterer_->getClusterLabels();

  EXPECT_EQ(labels.size(), 1);
  EXPECT_EQ(clusterer_->getLastHitCount(), 1);

  // With min_cluster_size = 1, single hit should form cluster
  EXPECT_EQ(num_clusters, 1);
  EXPECT_GE(labels[0], 0);  // Should be assigned to a cluster
}

// Test 3: Two nearby hits (should cluster)
TEST_F(SimpleABSClusteringTest, TwoNearbyHits) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 150),
      createHit(102, 101, 1002,
                120)  // Within 5-pixel radius and 50ns (2 TDC units)
  };

  [[maybe_unused]] size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end());
  const auto& labels = clusterer_->getClusterLabels();

  EXPECT_EQ(labels.size(), 2);
  EXPECT_EQ(clusterer_->getLastHitCount(), 2);

  // Both hits should be in same cluster (distance = sqrt((102-100)^2 +
  // (101-100)^2) = sqrt(5) < 5)
  EXPECT_EQ(num_clusters, 1);
  EXPECT_EQ(labels[0], labels[1]);
  EXPECT_GE(labels[0], 0);
}

// Test 4: Two distant hits (should not cluster)
TEST_F(SimpleABSClusteringTest, TwoDistantHits) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(200, 200,
                1010)  // Distance = sqrt(10000 + 10000) = ~141 pixels > 5
  };

  clusterer_->cluster(hits.begin(), hits.end());
  const auto& labels = clusterer_->getClusterLabels();

  EXPECT_EQ(labels.size(), 2);

  // Should form separate clusters or be unclustered
  EXPECT_NE(labels[0], labels[1]);
}

// Test 5: Box distance vs circular distance
TEST_F(SimpleABSClusteringTest, BoxDistanceCorrectness) {
  // Test box distance: max(|dx|, |dy|) <= radius
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Reference hit
      createHit(105, 103,
                1002),  // Box distance = max(5, 3) = 5 (should cluster)
      createHit(106, 102,
                1500),  // Box distance = max(6, 2) = 6 (should NOT cluster)
      createHit(103, 105,
                1002),  // Box distance = max(3, 5) = 5 (should cluster)
      createHit(102, 106,
                1500)  // Box distance = max(2, 6) = 6 (should NOT cluster)
  };

  clusterer_->cluster(hits.begin(), hits.end());
  const auto& labels = clusterer_->getClusterLabels();

  EXPECT_EQ(labels.size(), 5);

  // Hits 0, 1, 3 should be in same cluster (box distance <= 5)
  EXPECT_EQ(labels[0], labels[1]);
  EXPECT_EQ(labels[0], labels[3]);

  // Hits 2, 4 should be in different clusters or unclustered (box distance > 5)
  EXPECT_NE(labels[0], labels[2]);
  EXPECT_NE(labels[0], labels[4]);
}

// Test 6: Temporal correlation window
TEST_F(SimpleABSClusteringTest, TemporalCorrelation) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Reference time
      createHit(101, 101, 1002),  // 50ns later (within 75ns window)
      createHit(102, 102, 1005),  // 125ns later (outside 75ns window)
  };

  clusterer_->cluster(hits.begin(), hits.end());
  const auto& labels = clusterer_->getClusterLabels();

  EXPECT_EQ(labels.size(), 3);

  // First two hits should cluster (spatially close, temporally correlated)
  EXPECT_EQ(labels[0], labels[1]);

  // Third hit should not cluster with first (temporally too distant)
  EXPECT_NE(labels[0], labels[2]);
}

// Test 7: Large cluster formation
TEST_F(SimpleABSClusteringTest, LargeCluster) {
  std::vector<TDCHit> hits;

  // Create a 3x3 grid of hits (all within box distance of each other)
  for (int dx = 0; dx < 3; ++dx) {
    for (int dy = 0; dy < 3; ++dy) {
      hits.push_back(createHit(100 + dx * 2, 100 + dy * 2,
                               1000));  // Same time for all hits
    }
  }

  [[maybe_unused]] size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end());
  const auto& labels = clusterer_->getClusterLabels();

  EXPECT_EQ(labels.size(), 9);

  // All hits should be in the same cluster
  EXPECT_EQ(num_clusters, 1);
  for (size_t i = 1; i < labels.size(); ++i) {
    EXPECT_EQ(labels[0], labels[i]);
  }
}

// Test 8: Min cluster size filtering
TEST_F(SimpleABSClusteringTest, MinClusterSizeFiltering) {
  // Set minimum cluster size to 2
  config_.abs.min_cluster_size = 2;
  clusterer_->configure(config_);

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Isolated hit
      createHit(200, 200, 1100),  // Another isolated hit
      createHit(300, 300, 1200),  // Start of pair
      createHit(302, 301, 1202),  // Forms pair with previous (50ns later)
  };

  [[maybe_unused]] size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end());
  const auto& labels = clusterer_->getClusterLabels();

  EXPECT_EQ(labels.size(), 4);

  // Only the pair should form a valid cluster
  EXPECT_LE(num_clusters, 1);  // At most one cluster should be formed

  // First two hits should be unclustered (insufficient cluster size)
  EXPECT_EQ(labels[0], -1);
  EXPECT_EQ(labels[1], -1);

  // Last two hits should be clustered together
  EXPECT_EQ(labels[2], labels[3]);
  EXPECT_GE(labels[2], 0);
}

// Test 9: Multiple separate clusters
TEST_F(SimpleABSClusteringTest, MultipleClusters) {
  std::vector<TDCHit> hits = {
      // Cluster 1: 2x2 grid at (100,100)
      createHit(100, 100, 1000),
      createHit(102, 100, 1001),
      createHit(100, 102, 1002),
      createHit(102, 102, 1003),

      // Cluster 2: 2x2 grid at (200,200)
      createHit(200, 200, 1100),
      createHit(202, 200, 1101),
      createHit(200, 202, 1102),
      createHit(202, 202, 1103),

      // Isolated hit
      createHit(300, 300, 1200),
  };

  [[maybe_unused]] size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end());
  const auto& labels = clusterer_->getClusterLabels();

  EXPECT_EQ(labels.size(), 9);

  // Should have 3 clusters (two 2x2 grids + one isolated hit)
  EXPECT_EQ(num_clusters, 3);

  // Verify cluster 1 (hits 0-3)
  EXPECT_EQ(labels[0], labels[1]);
  EXPECT_EQ(labels[0], labels[2]);
  EXPECT_EQ(labels[0], labels[3]);

  // Verify cluster 2 (hits 4-7)
  EXPECT_EQ(labels[4], labels[5]);
  EXPECT_EQ(labels[4], labels[6]);
  EXPECT_EQ(labels[4], labels[7]);

  // Clusters should be different
  EXPECT_NE(labels[0], labels[4]);
  EXPECT_NE(labels[0], labels[8]);
  EXPECT_NE(labels[4], labels[8]);
}

// Test 10: Reset functionality
TEST_F(SimpleABSClusteringTest, ResetFunctionality) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(101, 101, 1001),
  };

  // Process hits
  [[maybe_unused]] size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end());
  EXPECT_GT(num_clusters, 0);
  EXPECT_EQ(clusterer_->getLastHitCount(), 2);

  // Reset
  clusterer_->reset();
  EXPECT_EQ(clusterer_->getLastHitCount(), 0);

  // Process same hits again - should get same result
  size_t num_clusters_after_reset =
      clusterer_->cluster(hits.begin(), hits.end());
  EXPECT_EQ(num_clusters, num_clusters_after_reset);
}

// Test 11: Configuration preservation during reset
TEST_F(SimpleABSClusteringTest, ConfigurationPreservation) {
  // Modify configuration
  config_.abs.radius = 10.0;
  clusterer_->configure(config_);

  // Verify configuration change
  EXPECT_EQ(clusterer_->getConfig().abs.radius, 10.0);

  // Reset and verify configuration is preserved
  clusterer_->reset();
  EXPECT_EQ(clusterer_->getConfig().abs.radius, 10.0);
  EXPECT_EQ(clusterer_->getConfig().abs.min_cluster_size, 1);
}

// Test 12: Statistics tracking
TEST_F(SimpleABSClusteringTest, StatisticsTracking) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(101, 101, 1001),
      createHit(200, 200, 1100),
  };

  [[maybe_unused]] size_t num_clusters =
      clusterer_->cluster(hits.begin(), hits.end());
  auto stats = clusterer_->getStatistics();

  EXPECT_EQ(stats.total_hits_processed, 3);
  // Note: processing_time_ms not implemented yet in basic version
  EXPECT_GE(stats.processing_time_ms, 0.0);
  EXPECT_LE(stats.total_clusters, num_clusters);
}

}  // namespace tdcsophiread