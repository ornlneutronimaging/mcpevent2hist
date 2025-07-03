// TDCSophiread ABS Clustering Algorithm Tests
// TDD approach: Tests for ABS clustering implementation

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <vector>

#include "tdc_abs_clustering.h"
#include "tdc_clustering_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

// Test class for ABS clustering
class TDCABSClusteringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test configuration with VENUS defaults
    config = ABSConfig{};
    config.radius = 5.0;
    config.min_cluster_size = 1;
    config.time_range_ns = 75.0;
    config.max_clusters = 8;

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

// Test 1: ABSClustering should handle empty input gracefully
TEST_F(TDCABSClusteringTest, HandlesEmptyInputGracefully) {
  std::vector<TDCHit> empty_hits;

  size_t num_clusters = abs_clustering->fit(empty_hits);

  EXPECT_EQ(num_clusters, 0);
  EXPECT_TRUE(abs_clustering->getClusterLabels().empty());

  auto stats = abs_clustering->getStatistics();
  EXPECT_EQ(stats.total_hits, 0);
  EXPECT_EQ(stats.total_clusters, 0);
}

// Test 2: ABSClustering should create single cluster for isolated hit
TEST_F(TDCABSClusteringTest, CreatesSingleClusterForIsolatedHit) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000)};

  size_t num_clusters = abs_clustering->fit(hits);

  EXPECT_EQ(num_clusters, 1);
  EXPECT_EQ(hits[0].cluster_id, 0);

  const auto& labels = abs_clustering->getClusterLabels();
  EXPECT_EQ(labels.size(), 1);
  EXPECT_EQ(labels[0], 0);

  auto stats = abs_clustering->getStatistics();
  EXPECT_EQ(stats.total_hits, 1);
  EXPECT_EQ(stats.total_clusters, 1);
  EXPECT_EQ(stats.single_hit_clusters, 1);
  EXPECT_EQ(stats.multi_hit_clusters, 0);
}

// Test 3: ABSClustering should merge spatially close hits
TEST_F(TDCABSClusteringTest, MergesSpatiallyCloseHits) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Cluster center
      createHit(102, 101, 1001),  // Within 5-pixel radius, 25ns later
      createHit(98, 99, 1002),    // Within 5-pixel radius, 50ns later
      createHit(103, 104, 1003)   // Within 5-pixel radius, 75ns later
  };

  size_t num_clusters = abs_clustering->fit(hits);

  EXPECT_EQ(num_clusters, 1);

  // All hits should have same cluster label
  for (const auto& hit : hits) {
    EXPECT_EQ(hit.cluster_id, 0);
  }

  auto stats = abs_clustering->getStatistics();
  EXPECT_EQ(stats.total_hits, 4);
  EXPECT_EQ(stats.total_clusters, 1);
  EXPECT_EQ(stats.single_hit_clusters, 0);
  EXPECT_EQ(stats.multi_hit_clusters, 1);
  EXPECT_DOUBLE_EQ(stats.mean_cluster_size, 4.0);
}

// Test 4: ABSClustering should separate spatially distant hits
TEST_F(TDCABSClusteringTest, SeparatesSpatiallyDistantHits) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Cluster 1
      createHit(120, 120, 1001),  // Cluster 2 (distance = ~28 pixels > 5)
      createHit(101, 101, 1002),  // Should join cluster 1
      createHit(121, 119, 1003)   // Should join cluster 2
  };

  size_t num_clusters = abs_clustering->fit(hits);

  EXPECT_EQ(num_clusters, 2);

  // First and third hits should have same label
  EXPECT_EQ(hits[0].cluster_id, hits[2].cluster_id);

  // Second and fourth hits should have same label (different from first)
  EXPECT_EQ(hits[1].cluster_id, hits[3].cluster_id);
  EXPECT_NE(hits[0].cluster_id, hits[1].cluster_id);

  auto stats = abs_clustering->getStatistics();
  EXPECT_EQ(stats.total_hits, 4);
  EXPECT_EQ(stats.total_clusters, 2);
  EXPECT_EQ(stats.single_hit_clusters, 0);
  EXPECT_EQ(stats.multi_hit_clusters, 2);
}

// Test 5: ABSClustering should merge temporally close hits
TEST_F(TDCABSClusteringTest, MergesTemporallyCloseHits) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Cluster center (time = 25μs)
      createHit(101, 101, 1002),  // +50ns within 75ns window
      createHit(99, 99, 998),     // -50ns within 75ns window
      createHit(102, 98, 1003)    // +75ns at edge of window
  };

  size_t num_clusters = abs_clustering->fit(hits);

  EXPECT_EQ(num_clusters, 1);

  // All hits should have same cluster label
  for (const auto& hit : hits) {
    EXPECT_EQ(hit.cluster_id, 0);
  }
}

// Test 6: ABSClustering should separate temporally distant hits
TEST_F(TDCABSClusteringTest, SeparatesTemporallyDistantHits) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Cluster 1 (time = 25μs)
      createHit(101, 101, 1004),  // Cluster 2 (time = 25.1μs > 75ns window)
      createHit(99, 99, 1001),    // Should join cluster 1
      createHit(102, 98, 1005)    // Should join cluster 2
  };

  size_t num_clusters = abs_clustering->fit(hits);

  EXPECT_EQ(num_clusters, 2);

  // First and third hits should have same label
  EXPECT_EQ(hits[0].cluster_id, hits[2].cluster_id);

  // Second and fourth hits should have same label (different from first)
  EXPECT_EQ(hits[1].cluster_id, hits[3].cluster_id);
  EXPECT_NE(hits[0].cluster_id, hits[1].cluster_id);
}

// Test 7: ABSClustering should handle 8-cluster limit with LRU replacement
TEST_F(TDCABSClusteringTest, HandlesClusterLimitWithLRUReplacen) {
  std::vector<TDCHit> hits;

  // Create 10 well-separated clusters (only 8 can be active)
  for (int i = 0; i < 10; ++i) {
    // Each cluster at different spatial and temporal location
    hits.push_back(createHit(i * 50, i * 50, 1000 + i * 100));  // Main hit
    hits.push_back(
        createHit(i * 50 + 1, i * 50 + 1, 1000 + i * 100 + 1));  // Second hit
  }

  size_t num_clusters = abs_clustering->fit(hits);

  // Should create 10 clusters total (8 active + 2 replaced)
  EXPECT_EQ(num_clusters, 10);

  auto stats = abs_clustering->getStatistics();
  EXPECT_EQ(stats.total_hits, 20);
  EXPECT_EQ(stats.total_clusters, 10);
  EXPECT_GT(stats.cluster_replacements, 0);  // Should have replacements
}

// Test 8: ABSClustering should assign correct cluster labels
TEST_F(TDCABSClusteringTest, AssignsCorrectClusterLabels) {
  std::vector<TDCHit> hits = {
      createHit(10, 10, 1000),  // Cluster 0
      createHit(50, 50, 2000),  // Cluster 1 (spatially distant)
      createHit(11, 11, 1001),  // Should join cluster 0
      createHit(51, 49, 2001),  // Should join cluster 1
  };

  size_t num_clusters = abs_clustering->fit(hits);

  EXPECT_EQ(num_clusters, 2);

  const auto& labels = abs_clustering->getClusterLabels();
  EXPECT_EQ(labels.size(), 4);

  // Check that hits are assigned to correct clusters
  EXPECT_EQ(labels[0], 0);  // First hit starts cluster 0
  EXPECT_EQ(labels[1], 1);  // Second hit starts cluster 1
  EXPECT_EQ(labels[2], 0);  // Third hit joins cluster 0
  EXPECT_EQ(labels[3], 1);  // Fourth hit joins cluster 1

  // Check that TDCHit objects have correct cluster_id
  EXPECT_EQ(hits[0].cluster_id, 0);
  EXPECT_EQ(hits[1].cluster_id, 1);
  EXPECT_EQ(hits[2].cluster_id, 0);
  EXPECT_EQ(hits[3].cluster_id, 1);
}

// Test 9: ABSClustering should reset state correctly
TEST_F(TDCABSClusteringTest, ResetsStateCorrectly) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000)};

  // First clustering
  abs_clustering->fit(hits);
  EXPECT_EQ(abs_clustering->getClusterLabels().size(), 1);

  // Reset and check state is cleared
  abs_clustering->reset();
  EXPECT_TRUE(abs_clustering->getClusterLabels().empty());

  auto stats = abs_clustering->getStatistics();
  EXPECT_EQ(stats.total_hits, 0);
  EXPECT_EQ(stats.total_clusters, 0);

  // Second clustering should start fresh
  std::vector<TDCHit> new_hits = {createHit(200, 200, 2000)};
  size_t num_clusters = abs_clustering->fit(new_hits);

  EXPECT_EQ(num_clusters, 1);
  EXPECT_EQ(new_hits[0].cluster_id, 0);  // Should restart from label 0
}

// Test 10: ABSClustering should update configuration correctly
TEST_F(TDCABSClusteringTest, UpdatesConfigurationCorrectly) {
  // Create hits that would cluster with radius=5 but not with radius=2
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(103, 103, 1001)  // Distance = sqrt(18) ≈ 4.2 pixels
  };

  // With radius=5, should form one cluster
  size_t num_clusters = abs_clustering->fit(hits);
  EXPECT_EQ(num_clusters, 1);

  // Update config to smaller radius
  ABSConfig new_config = config;
  new_config.radius = 2.0;
  abs_clustering->updateConfig(new_config);
  abs_clustering->reset();

  // Create fresh hits (reset clears cluster_id)
  hits = {createHit(100, 100, 1000), createHit(103, 103, 1001)};

  // With radius=2, should form two clusters
  num_clusters = abs_clustering->fit(hits);
  EXPECT_EQ(num_clusters, 2);
}

// Test 11: ABSClustering should handle edge cases
TEST_F(TDCABSClusteringTest, HandlesEdgeCases) {
  // Test with hits at spatial boundary - accounting for bounding box expansion
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(105, 100, 1001),  // 5 pixels away (should cluster)
      createHit(111, 100, 1002)  // 11 pixels from first, 6 from expanded bounds
                                 // (should not cluster)
  };

  size_t num_clusters = abs_clustering->fit(hits);

  EXPECT_EQ(num_clusters, 2);
  EXPECT_EQ(hits[0].cluster_id,
            hits[1].cluster_id);  // First two should cluster
  EXPECT_NE(hits[0].cluster_id,
            hits[2].cluster_id);  // Third should be separate
}

// Test 12: ABSClustering should provide performance statistics
TEST_F(TDCABSClusteringTest, ProvidesPerformanceStatistics) {
  std::vector<TDCHit> hits;

  // Create test dataset with well-separated hits (spatially and temporally)
  for (int i = 0; i < 100; ++i) {
    hits.push_back(createHit(i * 20, i * 20,
                             1000 + i * 10));  // 20-pixel spacing, 250ns apart
  }

  abs_clustering->fit(hits);
  auto stats = abs_clustering->getStatistics();

  EXPECT_EQ(stats.total_hits, 100);
  EXPECT_EQ(stats.total_clusters, 100);
  EXPECT_EQ(stats.single_hit_clusters, 100);
  EXPECT_EQ(stats.multi_hit_clusters, 0);
  EXPECT_DOUBLE_EQ(stats.mean_cluster_size, 1.0);
  EXPECT_GT(stats.processing_time_ms, 0.0);  // Should measure some time
}

// Test 13: ABSClustering should handle large datasets without overflow
TEST_F(TDCABSClusteringTest, HandlesLargeDatasetWithoutOverflow) {
  std::vector<TDCHit> hits;
  hits.reserve(100000);  // Reserve space for 100K hits

  // Create 100K well-separated hits (should create 100K clusters)
  std::random_device rd;
  std::mt19937 gen(42);  // Fixed seed for reproducible tests
  std::uniform_int_distribution<uint16_t> spatial_dist(0, 65535);
  std::uniform_int_distribution<uint32_t> temporal_dist(0, 1000000);

  for (size_t i = 0; i < 100000; ++i) {
    // Create spatially and temporally separated hits
    uint16_t x = spatial_dist(gen);
    uint16_t y = spatial_dist(gen);
    uint32_t tof = temporal_dist(gen);

    hits.push_back(createHit(x, y, tof));
  }

  // This should not crash or overflow
  size_t num_clusters = abs_clustering->fit(hits);

  auto stats = abs_clustering->getStatistics();

  EXPECT_EQ(stats.total_hits, 100000);
  EXPECT_GT(num_clusters, 0);
  EXPECT_LE(num_clusters, 100000);  // Can't have more clusters than hits

  // Verify no invalid cluster labels
  const auto& labels = abs_clustering->getClusterLabels();
  for (int label : labels) {
    EXPECT_GE(label, 0);  // All labels should be non-negative
    EXPECT_LT(label, static_cast<int>(num_clusters));  // Within valid range
  }
}

// Test 14: ABSClustering should handle overflow scenario gracefully
TEST_F(TDCABSClusteringTest, HandlesClusterOverflowGracefully) {
  // Create configuration that forces many small clusters
  ABSConfig stress_config = config;
  stress_config.radius = 0.1;         // Very small radius
  stress_config.time_range_ns = 0.1;  // Very small time window
  stress_config.max_clusters = 8;     // Limited active clusters

  auto stress_clustering = std::make_unique<ABSClustering>(stress_config);

  std::vector<TDCHit> hits;
  hits.reserve(50000);

  // Create hits that will each form their own cluster
  for (size_t i = 0; i < 50000; ++i) {
    // Space hits far apart to ensure separate clusters
    uint16_t x = static_cast<uint16_t>((i % 500) * 10);
    uint16_t y = static_cast<uint16_t>((i / 500) * 10);
    uint32_t tof = static_cast<uint32_t>(i * 100);  // 2.5μs apart

    hits.push_back(createHit(x, y, tof));
  }

  // This should create ~50K clusters without integer overflow
  size_t num_clusters = stress_clustering->fit(hits);

  auto stats = stress_clustering->getStatistics();

  EXPECT_EQ(stats.total_hits, 50000);
  EXPECT_GT(num_clusters, 32767);            // Should exceed old int16_t limit
  EXPECT_GT(stats.cluster_replacements, 0);  // Should have many replacements

  // Verify cluster labels are valid
  const auto& labels = stress_clustering->getClusterLabels();
  for (int label : labels) {
    EXPECT_GE(label, 0);
  }
}

}  // namespace tdcsophiread