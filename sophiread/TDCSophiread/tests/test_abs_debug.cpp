// Debug test for ABS clustering - simplified minimal cases
#include <gtest/gtest.h>

#include <iostream>

#include "tdc_abs_clustering.h"
#include "tdc_clustering_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

class TDCABSDebugTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config = ABSConfig{};
    config.radius = 5.0;
    config.min_cluster_size = 1;
    config.time_range_ns = 75.0;
    config.max_clusters = 8;

    abs_clustering = std::make_unique<ABSClustering>(config);
  }

  ABSConfig config;
  std::unique_ptr<ABSClustering> abs_clustering;

  TDCHit createHit(uint16_t x, uint16_t y, uint32_t tof, uint16_t tot = 100,
                   uint8_t chip_id = 0) {
    TDCHit hit;
    hit.x = x;
    hit.y = y;
    hit.tof = tof;
    hit.tot = tot;
    hit.chip_id = chip_id;
    hit.timestamp = tof;
    hit.cluster_id = -1;
    return hit;
  }
};

// Minimal debug test - just 2 hits that should definitely cluster
TEST_F(TDCABSDebugTest, TwoIdenticalHits) {
  std::cout << "\n=== DEBUG: Two Identical Hits Test ===\n";

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Hit 1
      createHit(100, 100, 1000)   // Hit 2 - identical
  };

  size_t num_clusters = abs_clustering->fit(hits);

  std::cout << "\nFinal result:\n";
  std::cout << "Number of clusters: " << num_clusters << "\n";
  std::cout << "Hit 0 cluster_id: " << (int)hits[0].cluster_id << "\n";
  std::cout << "Hit 1 cluster_id: " << (int)hits[1].cluster_id << "\n";

  // Two identical hits should definitely be in the same cluster
  EXPECT_EQ(num_clusters, 1);
  EXPECT_EQ(hits[0].cluster_id, hits[1].cluster_id);
}

// Next debug test - 2 hits slightly apart
TEST_F(TDCABSDebugTest, TwoNearbyHits) {
  std::cout << "\n=== DEBUG: Two Nearby Hits Test ===\n";

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Hit 1
      createHit(101, 101, 1001)   // Hit 2 - 1 pixel away, 25ns later
  };

  size_t num_clusters = abs_clustering->fit(hits);

  std::cout << "\nFinal result:\n";
  std::cout << "Number of clusters: " << num_clusters << "\n";
  std::cout << "Hit 0 cluster_id: " << (int)hits[0].cluster_id << "\n";
  std::cout << "Hit 1 cluster_id: " << (int)hits[1].cluster_id << "\n";

  // These should definitely cluster together
  EXPECT_EQ(num_clusters, 1);
  EXPECT_EQ(hits[0].cluster_id, hits[1].cluster_id);
}

// Debug the exact failing case from the main test
TEST_F(TDCABSDebugTest, OriginalFailingCase) {
  std::cout << "\n=== DEBUG: Original Failing Case ===\n";

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Cluster center
      createHit(102, 101, 1001),  // Within 5-pixel radius, 25ns later
      createHit(98, 99, 1002),    // Within 5-pixel radius, 50ns later
      createHit(103, 104, 1003)   // Within 5-pixel radius, 75ns later
  };

  size_t num_clusters = abs_clustering->fit(hits);

  std::cout << "\nFinal result:\n";
  std::cout << "Number of clusters: " << num_clusters << "\n";
  for (size_t i = 0; i < hits.size(); ++i) {
    std::cout << "Hit " << i << " cluster_id: " << (int)hits[i].cluster_id
              << "\n";
  }

  // This is the failing case - should be 1 cluster but we get 4
  EXPECT_EQ(num_clusters, 1);
}

}  // namespace tdcsophiread