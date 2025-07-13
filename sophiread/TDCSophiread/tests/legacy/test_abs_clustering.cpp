// TDCSophiread ABS Clustering Algorithm Tests
// Physics-correct implementation with time-based aging and min_cluster_size
// filtering

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "tdc_abs_clustering.h"
#include "tdc_clustering_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

// Test class for physics-correct ABS clustering
class TDCABSClusteringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test configuration with VENUS defaults
    config = ABSConfig{};
    config.radius = 5.0;
    config.min_cluster_size = 1;  // Default: any cluster is valid
    config.neutron_correlation_window =
        75.0;                    // 75ns temporal correlation window
    config.scan_interval = 100;  // Scan every 100 hits

    // Create ABS clustering instance
    abs_clustering = std::make_unique<ABSClustering>(config);
  }

  ABSConfig config;
  std::unique_ptr<ABSClustering> abs_clustering;

  // Helper to create test hits
  TDCHit createHit(uint16_t x, uint16_t y, uint32_t tof, uint16_t tot = 100,
                   uint8_t chip_id = 0) {
    TDCHit hit;
    hit.x = x;
    hit.y = y;
    hit.tof = tof;
    hit.tot = tot;
    hit.chip_id = chip_id;
    hit.timestamp = tof;
    hit.cluster_id = -1;  // Unassigned initially
    return hit;
  }
};

// Test 1: Empty input handling
TEST_F(TDCABSClusteringTest, HandlesEmptyInputGracefully) {
  std::vector<TDCHit> empty_hits;

  size_t num_clusters = abs_clustering->fit(empty_hits);

  EXPECT_EQ(num_clusters, 0);
  EXPECT_TRUE(abs_clustering->getClusterLabels().empty());
}

// Test 2: Single hit forms cluster with min_cluster_size=1
TEST_F(TDCABSClusteringTest, SingleHitFormsCluster) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000)};

  size_t num_clusters = abs_clustering->fit(hits);

  // With min_cluster_size=1, single hit forms valid cluster
  EXPECT_EQ(num_clusters, 1);
  EXPECT_EQ(hits[0].cluster_id, 0);
}

// Test 3: Test with higher min_cluster_size threshold
TEST_F(TDCABSClusteringTest, MinClusterSizeFiltering) {
  // Update config to require 3 hits
  config.min_cluster_size = 3;
  abs_clustering->updateConfig(config);

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Hit 1
      createHit(101, 101, 1001),  // Hit 2 - close in space and time
  };

  size_t num_clusters = abs_clustering->fit(hits);

  // With min_cluster_size=3, 2-hit cluster should remain unclustered
  EXPECT_EQ(num_clusters, 0);
  EXPECT_EQ(hits[0].cluster_id, -1);
  EXPECT_EQ(hits[1].cluster_id, -1);
}

// Test 4: Multiple hits form cluster with default min_cluster_size=1
TEST_F(TDCABSClusteringTest, MultipleHitsFormCluster) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Hit 1
      createHit(101, 101, 1001),  // Hit 2
      createHit(102, 99, 1002),   // Hit 3
  };

  size_t num_clusters = abs_clustering->fit(hits);

  // With min_cluster_size=1, all correlated hits form one cluster
  EXPECT_EQ(num_clusters, 1);
  EXPECT_EQ(hits[0].cluster_id, 0);
  EXPECT_EQ(hits[1].cluster_id, 0);
  EXPECT_EQ(hits[2].cluster_id, 0);
}

// Test 5: Time-based aging - hits outside spider_time_range form separate
// buckets
TEST_F(TDCABSClusteringTest, TimeBasedAging) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Bucket 1: t=25μs
      createHit(101, 101, 1001),  // Bucket 1: t=25.025μs
      createHit(102, 99, 1004),   // Bucket 2: t=25.1μs (>75ns gap)
      createHit(103, 98, 1005),   // Bucket 2: t=25.125μs
      createHit(104, 97, 1006),   // Bucket 2: t=25.15μs
  };

  size_t num_clusters = abs_clustering->fit(hits);

  // With min_cluster_size=1, both buckets form clusters
  EXPECT_EQ(num_clusters, 2);
  EXPECT_EQ(hits[0].cluster_id, 0);  // First bucket
  EXPECT_EQ(hits[1].cluster_id, 0);  // First bucket
  EXPECT_EQ(hits[2].cluster_id, 1);  // Second bucket
  EXPECT_EQ(hits[3].cluster_id, 1);  // Second bucket
  EXPECT_EQ(hits[4].cluster_id, 1);  // Second bucket
}

// Test 6: Spatial separation - hits outside radius form separate buckets
TEST_F(TDCABSClusteringTest, SpatialSeparation) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Bucket 1
      createHit(101, 101, 1001),  // Bucket 1
      createHit(102, 102, 1002),  // Bucket 1
      createHit(120, 120, 1003),  // Bucket 2 (spatially distant)
      createHit(121, 121, 1004),  // Bucket 2
      createHit(122, 119, 1005),  // Bucket 2
  };

  size_t num_clusters = abs_clustering->fit(hits);

  // Both buckets have 3 hits each, should form 2 valid clusters
  EXPECT_EQ(num_clusters, 2);

  // First 3 hits form one cluster
  EXPECT_EQ(hits[0].cluster_id, hits[1].cluster_id);
  EXPECT_EQ(hits[1].cluster_id, hits[2].cluster_id);

  // Last 3 hits form another cluster
  EXPECT_EQ(hits[3].cluster_id, hits[4].cluster_id);
  EXPECT_EQ(hits[4].cluster_id, hits[5].cluster_id);

  // Two clusters should have different IDs
  EXPECT_NE(hits[0].cluster_id, hits[3].cluster_id);
}

// Test 7: Gamma noise filtering with min_cluster_size > 1
TEST_F(TDCABSClusteringTest, GammaNoiseFiltering) {
  // Set min_cluster_size=3 for realistic neutron filtering
  config.min_cluster_size = 3;
  abs_clustering->updateConfig(config);

  std::vector<TDCHit> hits = {
      // Valid neutron cluster (3 hits)
      createHit(100, 100, 1000),
      createHit(101, 101, 1001),
      createHit(102, 99, 1002),

      // Gamma noise (2 hits - below threshold)
      createHit(200, 200, 2000),
      createHit(201, 201, 2001),

      // Another valid neutron cluster (4 hits)
      createHit(300, 300, 3000),
      createHit(301, 301, 3001),
      createHit(302, 299, 3002),
      createHit(303, 298, 3003),

      // Single gamma hit
      createHit(400, 400, 4000),
  };

  size_t num_clusters = abs_clustering->fit(hits);

  // Should form 2 valid neutron clusters, gamma noise remains unclustered
  EXPECT_EQ(num_clusters, 2);

  // First valid cluster
  EXPECT_NE(hits[0].cluster_id, -1);
  EXPECT_EQ(hits[0].cluster_id, hits[1].cluster_id);
  EXPECT_EQ(hits[1].cluster_id, hits[2].cluster_id);

  // Gamma noise remains unclustered
  EXPECT_EQ(hits[3].cluster_id, -1);
  EXPECT_EQ(hits[4].cluster_id, -1);

  // Second valid cluster
  EXPECT_NE(hits[5].cluster_id, -1);
  EXPECT_EQ(hits[5].cluster_id, hits[6].cluster_id);
  EXPECT_EQ(hits[6].cluster_id, hits[7].cluster_id);
  EXPECT_EQ(hits[7].cluster_id, hits[8].cluster_id);

  // Single gamma hit remains unclustered
  EXPECT_EQ(hits[9].cluster_id, -1);

  // Valid clusters should have different IDs
  EXPECT_NE(hits[0].cluster_id, hits[5].cluster_id);
}

// Test 8: TPX3 temporal disorder handling
TEST_F(TDCABSClusteringTest, HandlesTemporalDisorder) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1002),  // Hit arrives out of order
      createHit(101, 101, 1000),  // Earlier hit arrives later
      createHit(102, 99, 1001),   // Another out-of-order hit
      createHit(103, 98, 1003),   // Final hit in sequence
  };

  size_t num_clusters = abs_clustering->fit(hits);

  // All hits should be within spider_time_range and form one cluster
  EXPECT_EQ(num_clusters, 1);
  for (const auto& hit : hits) {
    EXPECT_EQ(hit.cluster_id, 0);
  }
}

// Test 9: Dynamic bucket growth - no space pressure
TEST_F(TDCABSClusteringTest, DynamicBucketGrowth) {
  std::vector<TDCHit> hits;

  // Create 20 spatially separated neutron clusters (3 hits each)
  for (int i = 0; i < 20; ++i) {
    uint16_t base_x = i * 50;  // Well separated spatially
    uint16_t base_y = i * 50;
    uint32_t base_tof = i * 1000;  // Well separated temporally

    hits.push_back(createHit(base_x, base_y, base_tof));
    hits.push_back(createHit(base_x + 1, base_y + 1, base_tof + 1));
    hits.push_back(createHit(base_x + 2, base_y + 2, base_tof + 2));
  }

  size_t num_clusters = abs_clustering->fit(hits);

  // Should form 20 valid neutron clusters (no space pressure)
  EXPECT_EQ(num_clusters, 20);

  // Verify all hits are properly clustered
  for (const auto& hit : hits) {
    EXPECT_NE(hit.cluster_id, -1);
  }
}

// Test 10: Configuration parameter effects
TEST_F(TDCABSClusteringTest, ConfigurationParameterEffects) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000), createHit(101, 101, 1001),
      createHit(102, 102, 1002), createHit(103, 103, 1003),
      createHit(104, 104, 1004),
  };

  // Test with min_cluster_size = 5
  ABSConfig strict_config = config;
  strict_config.min_cluster_size = 5;
  strict_config.neutron_correlation_window =
      100.0;  // 4 TDC units to fit all 5 hits
  abs_clustering->updateConfig(strict_config);

  size_t num_clusters = abs_clustering->fit(hits);

  // Should form one valid cluster with all 5 hits
  EXPECT_EQ(num_clusters, 1);
  for (const auto& hit : hits) {
    EXPECT_EQ(hit.cluster_id, 0);
  }

  // Test with min_cluster_size = 6 (too strict)
  strict_config.min_cluster_size = 6;
  strict_config.neutron_correlation_window = 100.0;  // Keep same window
  abs_clustering->updateConfig(strict_config);
  abs_clustering->reset();

  // Reset hits cluster_id for fresh test
  for (auto& hit : hits) {
    hit.cluster_id = -1;
  }

  num_clusters = abs_clustering->fit(hits);

  // Should remain unclustered
  EXPECT_EQ(num_clusters, 0);
  for (const auto& hit : hits) {
    EXPECT_EQ(hit.cluster_id, -1);
  }
}

// Test 11: Reset functionality
TEST_F(TDCABSClusteringTest, ResetFunctionality) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(101, 101, 1001),
      createHit(102, 102, 1002),
  };

  // First clustering
  size_t num_clusters = abs_clustering->fit(hits);
  EXPECT_EQ(num_clusters, 1);

  // Reset
  abs_clustering->reset();

  // Second clustering should start fresh
  for (auto& hit : hits) {
    hit.cluster_id = -1;  // Reset hit cluster IDs
  }

  num_clusters = abs_clustering->fit(hits);
  EXPECT_EQ(num_clusters, 1);
  EXPECT_EQ(hits[0].cluster_id, 0);  // Should start from 0 again
}

}  // namespace tdcsophiread