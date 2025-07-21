// TDCSophiread Simple Centroid Extraction Algorithm Tests
// Tests for TOT-weighted centroid calculation and neutron property extraction

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "neutron_processing/neutron_config.h"
#include "neutron_processing/simple_centroid_extraction.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Test fixture for Simple Centroid Extraction algorithm
 */
class SimpleCentroidExtractionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test configuration based on VENUS defaults
    config_ = NeutronExtractionConfig();
    config_.algorithm = "simple_centroid";
    config_.super_resolution_factor = 8.0;
    config_.weighted_by_tot = true;
    config_.min_tot_threshold = 10;

    // Create extraction instance
    extractor_ = std::make_unique<SimpleCentroidExtraction>(config_);
  }

  TDCHit createHit(uint16_t x, uint16_t y, uint32_t tof, uint16_t tot = 100,
                   uint8_t chip_id = 0) {
    TDCHit hit;
    hit.x = x;
    hit.y = y;
    hit.tof = tof;
    hit.tot = tot;
    hit.chip_id = chip_id;
    hit.timestamp = tof;
    return hit;
  }

  NeutronExtractionConfig config_;
  std::unique_ptr<SimpleCentroidExtraction> extractor_;
};

// Test 1: Empty input handling
TEST_F(SimpleCentroidExtractionTest, EmptyInput) {
  std::vector<TDCHit> empty_hits;
  std::vector<int> empty_labels;

  auto neutrons =
      extractor_->extract(empty_hits.begin(), empty_hits.end(), empty_labels);

  EXPECT_EQ(neutrons.size(), 0);
  auto stats = extractor_->getStatistics();
  EXPECT_EQ(stats.total_hits_processed, 0);
}

// Test 2: Single hit cluster
TEST_F(SimpleCentroidExtractionTest, SingleHitCluster) {
  std::vector<TDCHit> hits = {createHit(100, 150, 1000, 200, 1)};
  std::vector<int> labels = {0};  // Single cluster with ID 0

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);

  ASSERT_EQ(neutrons.size(), 1);

  const auto& neutron = neutrons[0];
  EXPECT_EQ(neutron.x, 100.0 * 8.0);  // Scaled by super-resolution factor
  EXPECT_EQ(neutron.y, 150.0 * 8.0);
  EXPECT_EQ(neutron.tof, 1000);
  EXPECT_EQ(neutron.tot, 200);
  EXPECT_EQ(neutron.n_hits, 1);
  EXPECT_EQ(neutron.chip_id, 1);
}

// Test 3: Two-hit cluster - TOT-weighted centroid
TEST_F(SimpleCentroidExtractionTest, TwoHitClusterWeighted) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 100),  // Lower TOT
      createHit(102, 102, 1001, 300),  // Higher TOT (3x weight)
  };
  std::vector<int> labels = {0, 0};  // Both in cluster 0

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);

  ASSERT_EQ(neutrons.size(), 1);

  const auto& neutron = neutrons[0];

  // Expected weighted centroid:
  // x = (100*100 + 102*300) / (100 + 300) = (10000 + 30600) / 400 = 101.5
  // y = (100*100 + 102*300) / (100 + 300) = (10000 + 30600) / 400 = 101.5
  // Then scaled by super-resolution factor (8.0)
  EXPECT_NEAR(neutron.x, 101.5 * 8.0, 1e-6);
  EXPECT_NEAR(neutron.y, 101.5 * 8.0, 1e-6);

  // TOF should be from hit with highest TOT (second hit)
  EXPECT_EQ(neutron.tof, 1001);

  // Combined TOT
  EXPECT_EQ(neutron.tot, 400);
  EXPECT_EQ(neutron.n_hits, 2);
}

// Test 4: Four-hit cluster - Complex weighting
TEST_F(SimpleCentroidExtractionTest, FourHitClusterWeighted) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 100),  // TOT = 100
      createHit(101, 100, 1001, 200),  // TOT = 200
      createHit(100, 101, 1002, 150),  // TOT = 150
      createHit(101, 101, 1003, 250),  // TOT = 250, highest TOT
  };
  std::vector<int> labels = {0, 0, 0, 0};  // All in cluster 0

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);

  ASSERT_EQ(neutrons.size(), 1);

  const auto& neutron = neutrons[0];

  // Manual calculation:
  // Total weight = 100 + 200 + 150 + 250 = 700
  // Weighted x = (100*100 + 101*200 + 100*150 + 101*250) / 700
  //            = (10000 + 20200 + 15000 + 25250) / 700 = 70450 / 700 ≈ 100.643
  // Weighted y = (100*100 + 100*200 + 101*150 + 101*250) / 700
  //            = (10000 + 20000 + 15150 + 25250) / 700 = 70400 / 700 ≈ 100.571
  // Then scaled by super-resolution factor (8.0)

  EXPECT_NEAR(neutron.x, 100.643 * 8.0, 0.01);
  EXPECT_NEAR(neutron.y, 100.571 * 8.0, 0.01);

  // TOF from highest TOT hit (4th hit)
  EXPECT_EQ(neutron.tof, 1003);

  // Combined TOT
  EXPECT_EQ(neutron.tot, 700);
  EXPECT_EQ(neutron.n_hits, 4);
}

// Test 5: Unweighted centroid (config option)
TEST_F(SimpleCentroidExtractionTest, UnweightedCentroid) {
  // Configure for unweighted (arithmetic mean)
  config_.weighted_by_tot = false;
  extractor_->configure(config_);

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 100),
      createHit(104, 104, 1001, 300),  // High TOT shouldn't affect centroid
  };
  std::vector<int> labels = {0, 0};

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);

  ASSERT_EQ(neutrons.size(), 1);

  const auto& neutron = neutrons[0];

  // Simple arithmetic mean: (100+104)/2 = 102, (100+104)/2 = 102
  // Then scaled by super-resolution factor (8.0)
  EXPECT_EQ(neutron.x, 102.0 * 8.0);
  EXPECT_EQ(neutron.y, 102.0 * 8.0);

  // TOF still from highest TOT hit
  EXPECT_EQ(neutron.tof, 1001);
  EXPECT_EQ(neutron.tot, 400);
}

// Test 6: Multiple clusters
TEST_F(SimpleCentroidExtractionTest, MultipleClusters) {
  std::vector<TDCHit> hits = {
      // Cluster 0
      createHit(100, 100, 1000, 100),
      createHit(101, 101, 1001, 200),

      // Cluster 1
      createHit(200, 200, 1100, 150),
      createHit(201, 201, 1101, 250),

      // Unclustered hit
      createHit(300, 300, 1200, 180),
  };
  std::vector<int> labels = {0, 0, 1, 1, -1};  // Two clusters + one unclustered

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);

  ASSERT_EQ(neutrons.size(), 2);  // Only clustered hits become neutrons

  // Verify cluster 0 neutron (scaled coordinates)
  auto cluster0_neutron =
      std::find_if(neutrons.begin(), neutrons.end(),
                   [](const TDCNeutron& n) { return n.x < 150.0 * 8.0; });
  ASSERT_NE(cluster0_neutron, neutrons.end());
  EXPECT_EQ(cluster0_neutron->n_hits, 2);

  // Verify cluster 1 neutron (scaled coordinates)
  auto cluster1_neutron =
      std::find_if(neutrons.begin(), neutrons.end(),
                   [](const TDCNeutron& n) { return n.x > 150.0 * 8.0; });
  ASSERT_NE(cluster1_neutron, neutrons.end());
  EXPECT_EQ(cluster1_neutron->n_hits, 2);
}

// Test 7: TOT filtering
TEST_F(SimpleCentroidExtractionTest, TOTFiltering) {
  // Set TOT threshold to 150
  config_.min_tot_threshold = 150;
  extractor_->configure(config_);

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 100),  // Below threshold - should be filtered
      createHit(101, 101, 1001, 200),  // Above threshold - should be kept
      createHit(102, 102, 1002, 120),  // Below threshold - should be filtered
      createHit(103, 103, 1003, 180),  // Above threshold - should be kept
  };
  std::vector<int> labels = {0, 0, 0, 0};  // All in same cluster

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);

  ASSERT_EQ(neutrons.size(), 1);

  const auto& neutron = neutrons[0];

  // Only hits with TOT >= 150 should contribute (hits 1 and 3)
  // Weighted centroid of (101,101,200) and (103,103,180):
  // x = (101*200 + 103*180) / (200+180) = (20200 + 18540) / 380 = 101.947
  // y = (101*200 + 103*180) / (200+180) = (20200 + 18540) / 380 = 101.947
  // Then scaled by super-resolution factor (8.0)

  EXPECT_NEAR(neutron.x, 101.947 * 8.0, 0.01);
  EXPECT_NEAR(neutron.y, 101.947 * 8.0, 0.01);

  // Combined TOT only from filtered hits
  EXPECT_EQ(neutron.tot, 380);

  // n_hits should reflect the filtered hits
  EXPECT_EQ(neutron.n_hits, 2);
}

// Test 8: Cluster rejected due to TOT filtering
TEST_F(SimpleCentroidExtractionTest, ClusterRejectedByTOTFiltering) {
  // Set high TOT threshold
  config_.min_tot_threshold = 500;
  extractor_->configure(config_);

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 100),  // Below threshold
      createHit(101, 101, 1001, 200),  // Below threshold
  };
  std::vector<int> labels = {0, 0};  // In cluster 0

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);

  // No neutrons should be produced (all hits filtered out)
  EXPECT_EQ(neutrons.size(), 0);

  auto stats = extractor_->getStatistics();
  EXPECT_EQ(stats.rejected_clusters, 1);
}

// Test 9: TOT overflow handling
TEST_F(SimpleCentroidExtractionTest, TOTOverflowHandling) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 60000),  // Large TOT
      createHit(101, 101, 1001, 30000),  // Large TOT
  };
  std::vector<int> labels = {0, 0};

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);

  ASSERT_EQ(neutrons.size(), 1);

  const auto& neutron = neutrons[0];

  // Combined TOT should be clamped to uint16_t max (65535)
  EXPECT_EQ(neutron.tot, 65535);
}

// Test 10: Representative TOF selection
TEST_F(SimpleCentroidExtractionTest, RepresentativeTOFSelection) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 100),  // TOT = 100
      createHit(101, 101, 1100,
                300),  // TOT = 300, highest - should be selected
      createHit(102, 102, 1200, 200),  // TOT = 200
  };
  std::vector<int> labels = {0, 0, 0};

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);

  ASSERT_EQ(neutrons.size(), 1);

  const auto& neutron = neutrons[0];

  // TOF should be from hit with highest TOT (second hit)
  EXPECT_EQ(neutron.tof, 1100);
}

// Test 11: Statistics tracking
TEST_F(SimpleCentroidExtractionTest, StatisticsTracking) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 100),
      createHit(101, 101, 1001, 200),
      createHit(200, 200, 1100, 150),
  };
  std::vector<int> labels = {0, 0, 1};  // Two clusters

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);
  auto stats = extractor_->getStatistics();

  EXPECT_EQ(stats.total_hits_processed, 3);
  EXPECT_EQ(stats.total_clusters_processed, 2);
  EXPECT_EQ(stats.total_neutrons_extracted, neutrons.size());
  EXPECT_GT(stats.processing_time_ms, 0.0);
  EXPECT_EQ(stats.rejected_clusters, 0);

  if (neutrons.size() > 0) {
    EXPECT_GT(stats.mean_hits_per_neutron, 0.0);
    EXPECT_GT(stats.extraction_efficiency, 0.0);
  }
}

// Test 12: Reset functionality
TEST_F(SimpleCentroidExtractionTest, ResetFunctionality) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000)};
  std::vector<int> labels = {0};

  // Process hits
  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);
  auto stats_before = extractor_->getStatistics();
  EXPECT_GT(stats_before.total_hits_processed, 0);

  // Reset
  extractor_->reset();
  auto stats_after = extractor_->getStatistics();
  EXPECT_EQ(stats_after.total_hits_processed, 0);

  // Configuration should be preserved
  EXPECT_EQ(extractor_->getConfig().algorithm, "simple_centroid");
}

// Test 13: Size mismatch handling
TEST_F(SimpleCentroidExtractionTest, SizeMismatchHandling) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000),
                              createHit(101, 101, 1001)};
  std::vector<int> labels = {0};  // Wrong size - only one label for two hits

  auto neutrons = extractor_->extract(hits.begin(), hits.end(), labels);

  // Should handle gracefully - return empty result
  EXPECT_EQ(neutrons.size(), 0);
}

// Test 14: Super resolution factor access
TEST_F(SimpleCentroidExtractionTest, SuperResolutionFactor) {
  EXPECT_EQ(extractor_->getSuperResolutionFactor(), 8.0);

  // Change configuration
  config_.super_resolution_factor = 16.0;
  extractor_->configure(config_);
  EXPECT_EQ(extractor_->getSuperResolutionFactor(), 16.0);
}

}  // namespace tdcsophiread