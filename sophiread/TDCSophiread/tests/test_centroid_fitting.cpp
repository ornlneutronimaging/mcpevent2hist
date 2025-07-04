// TDCSophiread Centroid Peak Fitting Tests
// TDD approach: Tests for TOT-weighted centroid calculation

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "tdc_centroid_fitting.h"
#include "tdc_clustering_config.h"

namespace tdcsophiread {

// Test class for CentroidPeakFitting
class TDCCentroidFittingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test configuration with VENUS defaults
    config_ = ClusteringConfig::venusDefaults().centroid;

    // Create test hits with known cluster assignments
    test_hits_single_cluster = {
        // Cluster 0: 2x2 cluster centered at (10, 20)
        TDCHit(1000, 9, 19, 1000, 100, 0),   // Bottom-left
        TDCHit(1001, 10, 19, 1001, 150, 0),  // Bottom-right
        TDCHit(1002, 9, 20, 1002, 120, 0),   // Top-left
        TDCHit(1003, 10, 20, 1003, 200, 0),  // Top-right (highest TOT)
    };

    // Assign all hits to cluster 0
    for (auto& hit : test_hits_single_cluster) {
      hit.cluster_id = 0;
    }

    test_hits_multiple_clusters = {
        // Cluster 0: Single hit
        TDCHit(2000, 50, 60, 2000, 80, 0),

        // Cluster 1: Linear cluster (3 hits)
        TDCHit(3000, 100, 200, 3000, 100, 1),
        TDCHit(3001, 101, 200, 3001, 200, 1),  // Highest TOT
        TDCHit(3002, 102, 200, 3002, 150, 1),

        // Cluster 2: Compact cluster (4 hits)
        TDCHit(4000, 300, 300, 4000, 120, 2),
        TDCHit(4001, 300, 301, 4001, 140, 2),
        TDCHit(4002, 301, 300, 4002, 160, 2),
        TDCHit(4003, 301, 301, 4003, 180, 2),  // Highest TOT
    };

    // Assign cluster IDs
    test_hits_multiple_clusters[0].cluster_id = 0;  // Single hit
    test_hits_multiple_clusters[1].cluster_id = 1;  // Linear cluster
    test_hits_multiple_clusters[2].cluster_id = 1;
    test_hits_multiple_clusters[3].cluster_id = 1;
    test_hits_multiple_clusters[4].cluster_id = 2;  // Compact cluster
    test_hits_multiple_clusters[5].cluster_id = 2;
    test_hits_multiple_clusters[6].cluster_id = 2;
    test_hits_multiple_clusters[7].cluster_id = 2;
  }

  CentroidConfig config_;
  std::vector<TDCHit> test_hits_single_cluster;
  std::vector<TDCHit> test_hits_multiple_clusters;
};

// Test 1: CentroidPeakFitting should construct with valid configuration
TEST_F(TDCCentroidFittingTest, ConstructsWithValidConfiguration) {
  EXPECT_NO_THROW(CentroidPeakFitting fitting(config_));

  CentroidPeakFitting fitting(config_);
  EXPECT_EQ(fitting.getName(), "centroid");
}

// Test 2: CentroidPeakFitting should handle empty hit list
TEST_F(TDCCentroidFittingTest, HandlesEmptyHitList) {
  CentroidPeakFitting fitting(config_);
  std::vector<TDCHit> empty_hits;

  auto neutrons = fitting.extractNeutrons(empty_hits);

  EXPECT_TRUE(neutrons.empty());
  EXPECT_EQ(fitting.getLastHitCount(), 0);

  auto stats = fitting.getStatistics();
  EXPECT_EQ(stats.total_hits_processed, 0);
  EXPECT_EQ(stats.neutrons_extracted, 0);
  EXPECT_EQ(stats.total_clusters_found, 0);
}

// Test 3: CentroidPeakFitting should handle single hit cluster
TEST_F(TDCCentroidFittingTest, HandlesSingleHitCluster) {
  CentroidPeakFitting fitting(config_);

  // Create single hit
  std::vector<TDCHit> single_hit = {TDCHit(1500, 100, 200, 1500, 120, 1)};
  single_hit[0].cluster_id = 5;

  auto neutrons = fitting.extractNeutrons(single_hit);

  ASSERT_EQ(neutrons.size(), 1);

  // Check coordinates (should be in native pixel space - no super-resolution)
  EXPECT_DOUBLE_EQ(neutrons[0].x, 100.0);
  EXPECT_DOUBLE_EQ(neutrons[0].y, 200.0);
  EXPECT_EQ(neutrons[0].tof, 1500);
  EXPECT_EQ(neutrons[0].tot, 120);
  EXPECT_EQ(neutrons[0].n_hits, 1);
  EXPECT_EQ(neutrons[0].chip_id, 1);

  // Check statistics
  auto stats = fitting.getStatistics();
  EXPECT_EQ(stats.single_hit_neutrons, 1);
  EXPECT_EQ(stats.multi_hit_neutrons, 0);
}

// Test 4: CentroidPeakFitting should calculate TOT-weighted centroid correctly
TEST_F(TDCCentroidFittingTest, CalculatesTOTWeightedCentroidCorrectly) {
  CentroidPeakFitting fitting(config_);

  auto neutrons = fitting.extractNeutrons(test_hits_single_cluster);

  ASSERT_EQ(neutrons.size(), 1);

  // Calculate expected TOT-weighted centroid manually
  // Hits: (9,19,TOT=100), (10,19,TOT=150), (9,20,TOT=120), (10,20,TOT=200)
  // Total TOT = 100 + 150 + 120 + 200 = 570
  // Weighted X = (9*100 + 10*150 + 9*120 + 10*200) / 570 =
  // (900+1500+1080+2000)/570 = 5480/570 ≈ 9.614 Weighted Y = (19*100 + 19*150 +
  // 20*120 + 20*200) / 570 = (1900+2850+2400+4000)/570 = 11150/570 ≈ 19.561

  double expected_x = 9.614035087719298;  // Precise calculation
  double expected_y = 19.561403508771930;

  EXPECT_NEAR(neutrons[0].x, expected_x, 1e-6);
  EXPECT_NEAR(neutrons[0].y, expected_y, 1e-6);
  EXPECT_EQ(neutrons[0].n_hits, 4);
  EXPECT_EQ(neutrons[0].tot, 100 + 150 + 120 + 200);  // Combined TOT
  EXPECT_EQ(neutrons[0].tof, 1003);  // TOF from hit with highest TOT (200)
}

// Test 5: CentroidPeakFitting should handle unweighted centroid calculation
TEST_F(TDCCentroidFittingTest, HandlesUnweightedCentroidCalculation) {
  config_.weighted_by_tot = false;  // Disable TOT weighting
  CentroidPeakFitting fitting(config_);

  auto neutrons = fitting.extractNeutrons(test_hits_single_cluster);

  ASSERT_EQ(neutrons.size(), 1);

  // Calculate expected arithmetic mean
  // X = (9 + 10 + 9 + 10) / 4 = 38/4 = 9.5
  // Y = (19 + 19 + 20 + 20) / 4 = 78/4 = 19.5

  EXPECT_DOUBLE_EQ(neutrons[0].x, 9.5);
  EXPECT_DOUBLE_EQ(neutrons[0].y, 19.5);
  EXPECT_EQ(neutrons[0].n_hits, 4);
}

// Test 6: CentroidPeakFitting should process multiple clusters correctly
TEST_F(TDCCentroidFittingTest, ProcessesMultipleClustersCorrectly) {
  CentroidPeakFitting fitting(config_);

  auto neutrons = fitting.extractNeutrons(test_hits_multiple_clusters);

  EXPECT_EQ(neutrons.size(), 3);  // 3 clusters

  // Sort neutrons by cluster size for predictable testing
  std::sort(neutrons.begin(), neutrons.end(),
            [](const TDCNeutron& a, const TDCNeutron& b) {
              return a.n_hits < b.n_hits;
            });

  // Check single hit neutron (cluster 0)
  EXPECT_EQ(neutrons[0].n_hits, 1);
  EXPECT_DOUBLE_EQ(neutrons[0].x, 50.0);
  EXPECT_DOUBLE_EQ(neutrons[0].y, 60.0);

  // Check 3-hit linear cluster (cluster 1)
  EXPECT_EQ(neutrons[1].n_hits, 3);
  EXPECT_EQ(neutrons[1].chip_id, 1);

  // Check 4-hit compact cluster (cluster 2)
  EXPECT_EQ(neutrons[2].n_hits, 4);
  EXPECT_EQ(neutrons[2].chip_id, 2);

  // Check statistics
  auto stats = fitting.getStatistics();
  EXPECT_EQ(stats.single_hit_neutrons, 1);
  EXPECT_EQ(stats.multi_hit_neutrons, 2);
  EXPECT_EQ(stats.total_clusters_found, 3);
}

// Test 7: CentroidPeakFitting should apply TOT threshold filtering
TEST_F(TDCCentroidFittingTest, AppliesToTThresholdFiltering) {
  config_.min_tot_threshold = 130.0;  // Filter hits with TOT < 130
  CentroidPeakFitting fitting(config_);

  auto neutrons = fitting.extractNeutrons(test_hits_single_cluster);

  // Only hits with TOT >= 130 should remain: (10,19,TOT=150), (10,20,TOT=200)
  // Expected centroid: X = (10*150 + 10*200)/(150+200) = 3500/350 = 10.0
  // Expected centroid: Y = (19*150 + 20*200)/(150+200) = (2850+4000)/350 =
  // 6850/350 = 19.571

  ASSERT_EQ(neutrons.size(), 1);
  EXPECT_DOUBLE_EQ(neutrons[0].x, 10.0);
  EXPECT_NEAR(neutrons[0].y, 19.571428571428573, 1e-6);
  EXPECT_EQ(neutrons[0].n_hits, 2);       // Only 2 hits after filtering
  EXPECT_EQ(neutrons[0].tot, 150 + 200);  // Combined TOT of filtered hits

  // Check filtering statistics
  auto stats = fitting.getStatistics();
  EXPECT_EQ(stats.hits_below_threshold, 2);  // 2 hits filtered out
}

// Test 8: CentroidPeakFitting should handle unclustered hits gracefully
TEST_F(TDCCentroidFittingTest, HandlesUnclusteredHitsGracefully) {
  CentroidPeakFitting fitting(config_);

  // Create hits with cluster_id = -1 (unclustered)
  std::vector<TDCHit> unclustered_hits = {
      TDCHit(1000, 100, 200, 1000, 100, 0),
      TDCHit(1500, 150, 250, 1500, 120, 1),
  };

  for (auto& hit : unclustered_hits) {
    hit.cluster_id = -1;  // Mark as unclustered
  }

  auto neutrons = fitting.extractNeutrons(unclustered_hits);

  EXPECT_TRUE(neutrons.empty());  // No neutrons from unclustered hits

  auto stats = fitting.getStatistics();
  EXPECT_EQ(stats.neutrons_extracted, 0);
  EXPECT_EQ(stats.total_clusters_found, 1);  // Still counts the -1 "cluster"
}

// Test 9: CentroidPeakFitting should handle mixed clustered/unclustered hits
TEST_F(TDCCentroidFittingTest, HandlesMixedClusteredUnclusteredHits) {
  CentroidPeakFitting fitting(config_);

  std::vector<TDCHit> mixed_hits = {
      TDCHit(1000, 100, 200, 1000, 100, 0),  // Clustered
      TDCHit(1001, 101, 200, 1001, 120, 0),  // Clustered
      TDCHit(2000, 200, 300, 2000, 80, 1),   // Unclustered
  };

  mixed_hits[0].cluster_id = 5;   // Valid cluster
  mixed_hits[1].cluster_id = 5;   // Same cluster
  mixed_hits[2].cluster_id = -1;  // Unclustered

  auto neutrons = fitting.extractNeutrons(mixed_hits);

  EXPECT_EQ(neutrons.size(), 1);  // Only one neutron from cluster 5
  EXPECT_EQ(neutrons[0].n_hits, 2);

  auto stats = fitting.getStatistics();
  EXPECT_EQ(stats.neutrons_extracted, 1);
  EXPECT_EQ(stats.total_clusters_found, 2);  // Cluster 5 and cluster -1
}

// Test 10: CentroidPeakFitting should update configuration correctly
TEST_F(TDCCentroidFittingTest, UpdatesConfigurationCorrectly) {
  CentroidPeakFitting fitting(config_);

  // Update configuration
  CentroidConfig new_config = config_;
  new_config.super_resolution_factor = 16.0;
  new_config.weighted_by_tot = false;

  fitting.updateConfig(new_config);

  // Test with updated configuration
  std::vector<TDCHit> single_hit = {TDCHit(1000, 10, 20, 1000, 100, 0)};
  single_hit[0].cluster_id = 0;

  auto neutrons = fitting.extractNeutrons(single_hit);

  ASSERT_EQ(neutrons.size(), 1);
  EXPECT_DOUBLE_EQ(neutrons[0].x,
                   10.0);  // No super-resolution in centroid fitting
  EXPECT_DOUBLE_EQ(neutrons[0].y, 20.0);
}

// Test 11: CentroidPeakFitting should configure from ClusteringConfig
TEST_F(TDCCentroidFittingTest, ConfiguresFromClusteringConfig) {
  CentroidPeakFitting fitting(config_);

  auto clustering_config = ClusteringConfig::venusDefaults();
  clustering_config.centroid.super_resolution_factor = 12.0;

  fitting.configure(clustering_config);

  // Test with updated configuration
  std::vector<TDCHit> single_hit = {TDCHit(1000, 5, 10, 1000, 100, 0)};
  single_hit[0].cluster_id = 0;

  auto neutrons = fitting.extractNeutrons(single_hit);

  ASSERT_EQ(neutrons.size(), 1);
  EXPECT_DOUBLE_EQ(neutrons[0].x,
                   5.0);  // No super-resolution in centroid fitting
  EXPECT_DOUBLE_EQ(neutrons[0].y, 10.0);
}

// Test 12: CentroidPeakFitting should reset statistics correctly
TEST_F(TDCCentroidFittingTest, ResetsStatisticsCorrectly) {
  CentroidPeakFitting fitting(config_);

  // Process some hits to generate statistics
  fitting.extractNeutrons(test_hits_single_cluster);

  auto initial_stats = fitting.getStatistics();
  EXPECT_GT(initial_stats.total_hits_processed, 0);

  // Reset and verify
  fitting.reset();

  auto reset_stats = fitting.getStatistics();
  EXPECT_EQ(reset_stats.total_hits_processed, 0);
  EXPECT_EQ(reset_stats.neutrons_extracted, 0);
  EXPECT_EQ(reset_stats.total_clusters_found, 0);
  EXPECT_DOUBLE_EQ(reset_stats.processing_time_ms, 0.0);
}

// Test 13: CentroidPeakFitting should provide detailed statistics
TEST_F(TDCCentroidFittingTest, ProvidesDetailedStatistics) {
  CentroidPeakFitting fitting(config_);

  auto neutrons = fitting.extractNeutrons(test_hits_multiple_clusters);
  auto stats = fitting.getStatistics();

  // Verify comprehensive statistics
  EXPECT_EQ(stats.total_hits_processed, test_hits_multiple_clusters.size());
  EXPECT_EQ(stats.neutrons_extracted, 3);
  EXPECT_EQ(stats.total_clusters_found, 3);
  EXPECT_EQ(stats.hits_below_threshold, 0);  // No filtering applied
  EXPECT_EQ(stats.single_hit_neutrons, 1);
  EXPECT_EQ(stats.multi_hit_neutrons, 2);
  EXPECT_GT(stats.processing_time_ms, 0.0);  // Should have some processing time

  // Check mean calculations
  double expected_mean_cluster_size = (1.0 + 3.0 + 4.0) / 3.0;  // 8/3 = 2.67
  EXPECT_NEAR(stats.mean_cluster_size, expected_mean_cluster_size, 1e-6);

  // Mean TOT should be positive
  EXPECT_GT(stats.mean_tot_weight, 0.0);
}

// Test 14: CentroidPeakFitting should handle TOT overflow gracefully
TEST_F(TDCCentroidFittingTest, HandlesTOTOverflowGracefully) {
  CentroidPeakFitting fitting(config_);

  // Create hits with very high TOT values that would overflow uint16_t when
  // summed
  std::vector<TDCHit> high_tot_hits = {
      TDCHit(1000, 100, 200, 1000, 60000, 0),  // High TOT
      TDCHit(1001, 101, 200, 1001, 60000, 0),  // High TOT
  };

  for (auto& hit : high_tot_hits) {
    hit.cluster_id = 0;
  }

  auto neutrons = fitting.extractNeutrons(high_tot_hits);

  ASSERT_EQ(neutrons.size(), 1);

  // TOT should be clamped to uint16_t max (65535)
  EXPECT_EQ(neutrons[0].tot, 65535);
  EXPECT_EQ(neutrons[0].n_hits, 2);
}

// Test 15: CentroidPeakFitting should handle super-resolution scaling correctly
TEST_F(TDCCentroidFittingTest, HandlesSupeResolutionScalingCorrectly) {
  // Test with different super-resolution factors
  std::vector<double> test_factors = {1.0, 2.0, 4.0, 8.0, 16.0};

  for (double factor : test_factors) {
    config_.super_resolution_factor = factor;
    CentroidPeakFitting fitting(config_);

    std::vector<TDCHit> single_hit = {TDCHit(1000, 10, 20, 1000, 100, 0)};
    single_hit[0].cluster_id = 0;

    auto neutrons = fitting.extractNeutrons(single_hit);

    ASSERT_EQ(neutrons.size(), 1);
    EXPECT_DOUBLE_EQ(neutrons[0].x,
                     10.0);  // No super-resolution in centroid fitting
    EXPECT_DOUBLE_EQ(neutrons[0].y, 20.0);
  }
}

}  // namespace tdcsophiread