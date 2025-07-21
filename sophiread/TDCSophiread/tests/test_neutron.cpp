// TDCSophiread Neutron Data Structure Tests
// TDD approach: Tests for TDCNeutron structure and utilities

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "tdc_neutron.h"

namespace tdcsophiread {

// Test class for TDCNeutron
class TDCNeutronTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test neutron events with varied properties
    test_neutrons = {
        TDCNeutron(100.5, 200.5, 1000, 50, 1, 0),  // Single hit neutron, chip 0
        TDCNeutron(150.25, 250.75, 2000, 120, 4, 1),  // Small cluster, chip 1
        TDCNeutron(300.0, 400.0, 3000, 200, 8, 2),    // Medium cluster, chip 2
        TDCNeutron(50.1, 75.9, 4000, 350, 15, 3),     // Large cluster, chip 3
        TDCNeutron(200.0, 300.0, 1500, 80, 2, 0),     // Small cluster, chip 0
    };
  }

  std::vector<TDCNeutron> test_neutrons;
};

// Test 1: TDCNeutron should have correct default constructor
TEST_F(TDCNeutronTest, HasCorrectDefaultConstructor) {
  TDCNeutron neutron;

  EXPECT_EQ(neutron.x, 0.0);
  EXPECT_EQ(neutron.y, 0.0);
  EXPECT_EQ(neutron.tof, 0);
  EXPECT_EQ(neutron.tot, 0);
  EXPECT_EQ(neutron.n_hits, 0);
  EXPECT_EQ(neutron.chip_id, 0);
  EXPECT_EQ(neutron.reserved, 0);
}

// Test 2: TDCNeutron should have correct parameterized constructor
TEST_F(TDCNeutronTest, HasCorrectParameterizedConstructor) {
  TDCNeutron neutron(123.45, 678.90, 5000, 250, 12, 2);

  EXPECT_DOUBLE_EQ(neutron.x, 123.45);
  EXPECT_DOUBLE_EQ(neutron.y, 678.90);
  EXPECT_EQ(neutron.tof, 5000);
  EXPECT_EQ(neutron.tot, 250);
  EXPECT_EQ(neutron.n_hits, 12);
  EXPECT_EQ(neutron.chip_id, 2);
  EXPECT_EQ(neutron.reserved, 0);
}

// Test 3: TDCNeutron should convert TOF units correctly
TEST_F(TDCNeutronTest, ConvertsTOFUnitsCorrectly) {
  TDCNeutron neutron(0.0, 0.0, 4000, 0, 1, 0);  // 4000 * 25ns = 100,000ns

  // Test nanoseconds conversion
  EXPECT_DOUBLE_EQ(neutron.getTOFNanoseconds(), 100000.0);

  // Test milliseconds conversion
  EXPECT_DOUBLE_EQ(neutron.getTOFMilliseconds(), 0.1);
}

// Test 4: TDCNeutron should identify chip correctly
TEST_F(TDCNeutronTest, IdentifiesChipCorrectly) {
  TDCNeutron neutron(0.0, 0.0, 1000, 50, 1, 2);

  EXPECT_TRUE(neutron.isFromChip(2));
  EXPECT_FALSE(neutron.isFromChip(0));
  EXPECT_FALSE(neutron.isFromChip(1));
  EXPECT_FALSE(neutron.isFromChip(3));
}

// Test 5: TDCNeutron should categorize cluster sizes correctly
TEST_F(TDCNeutronTest, CategorizesClusterSizesCorrectly) {
  // Test single hit
  TDCNeutron single(0.0, 0.0, 1000, 50, 1, 0);
  EXPECT_EQ(single.getClusterSizeCategory(), TDCNeutron::ClusterSize::Single);

  // Test small cluster (2-4 hits)
  TDCNeutron small2(0.0, 0.0, 1000, 50, 2, 0);
  TDCNeutron small4(0.0, 0.0, 1000, 50, 4, 0);
  EXPECT_EQ(small2.getClusterSizeCategory(), TDCNeutron::ClusterSize::Small);
  EXPECT_EQ(small4.getClusterSizeCategory(), TDCNeutron::ClusterSize::Small);

  // Test medium cluster (5-10 hits)
  TDCNeutron medium5(0.0, 0.0, 1000, 50, 5, 0);
  TDCNeutron medium10(0.0, 0.0, 1000, 50, 10, 0);
  EXPECT_EQ(medium5.getClusterSizeCategory(), TDCNeutron::ClusterSize::Medium);
  EXPECT_EQ(medium10.getClusterSizeCategory(), TDCNeutron::ClusterSize::Medium);

  // Test large cluster (>10 hits)
  TDCNeutron large(0.0, 0.0, 1000, 50, 15, 0);
  EXPECT_EQ(large.getClusterSizeCategory(), TDCNeutron::ClusterSize::Large);
}

// Test 6: TDCNeutron should calculate spatial distance correctly
TEST_F(TDCNeutronTest, CalculatesSpatialDistanceCorrectly) {
  TDCNeutron neutron1(0.0, 0.0, 1000, 50, 1, 0);
  TDCNeutron neutron2(3.0, 4.0, 1500, 60, 1, 0);

  // Distance should be sqrt(3^2 + 4^2) = 5.0
  EXPECT_DOUBLE_EQ(neutron1.distanceTo(neutron2), 5.0);
  EXPECT_DOUBLE_EQ(neutron2.distanceTo(neutron1), 5.0);

  // Distance to self should be 0
  EXPECT_DOUBLE_EQ(neutron1.distanceTo(neutron1), 0.0);
}

// Test 7: TDCNeutron should calculate temporal distance correctly
TEST_F(TDCNeutronTest, CalculatesTemporalDistanceCorrectly) {
  TDCNeutron neutron1(0.0, 0.0, 1000, 50, 1, 0);
  TDCNeutron neutron2(0.0, 0.0, 1500, 60, 1, 0);

  // Time difference should be |1500 - 1000| = 500
  EXPECT_EQ(neutron1.timeDifferenceTo(neutron2), 500);
  EXPECT_EQ(neutron2.timeDifferenceTo(neutron1), 500);

  // Time difference to self should be 0
  EXPECT_EQ(neutron1.timeDifferenceTo(neutron1), 0);
}

// Test 8: NeutronStatistics should have correct default constructor
TEST_F(TDCNeutronTest, NeutronStatisticsHasCorrectDefaults) {
  NeutronStatistics stats;

  EXPECT_EQ(stats.total_neutrons, 0);
  EXPECT_EQ(stats.single_hit_neutrons, 0);
  EXPECT_EQ(stats.multi_hit_neutrons, 0);
  EXPECT_DOUBLE_EQ(stats.mean_cluster_size, 0.0);
  EXPECT_DOUBLE_EQ(stats.mean_tot, 0.0);
  EXPECT_EQ(stats.tof_min, 0);
  EXPECT_EQ(stats.tof_max, 0);
  EXPECT_DOUBLE_EQ(stats.x_min, 0.0);
  EXPECT_DOUBLE_EQ(stats.x_max, 0.0);
  EXPECT_DOUBLE_EQ(stats.y_min, 0.0);
  EXPECT_DOUBLE_EQ(stats.y_max, 0.0);
  EXPECT_EQ(stats.chip_counts.size(), 4);
  for (auto count : stats.chip_counts) {
    EXPECT_EQ(count, 0);
  }
}

// Test 9: NeutronUtils should calculate statistics correctly
TEST_F(TDCNeutronTest, NeutronUtilsCalculatesStatisticsCorrectly) {
  auto stats = NeutronUtils::calculateStatistics(test_neutrons);

  EXPECT_EQ(stats.total_neutrons, 5);
  EXPECT_EQ(stats.single_hit_neutrons, 1);  // Only first neutron has 1 hit
  EXPECT_EQ(stats.multi_hit_neutrons, 4);   // Other 4 have multiple hits

  // Check mean cluster size: (1 + 4 + 8 + 15 + 2) / 5 = 6.0
  EXPECT_DOUBLE_EQ(stats.mean_cluster_size, 6.0);

  // Check mean TOT: (50 + 120 + 200 + 350 + 80) / 5 = 160.0
  EXPECT_DOUBLE_EQ(stats.mean_tot, 160.0);

  // Check TOF range
  EXPECT_EQ(stats.tof_min, 1000);
  EXPECT_EQ(stats.tof_max, 4000);

  // Check coordinate ranges
  EXPECT_DOUBLE_EQ(stats.x_min, 50.1);
  EXPECT_DOUBLE_EQ(stats.x_max, 300.0);
  EXPECT_DOUBLE_EQ(stats.y_min, 75.9);
  EXPECT_DOUBLE_EQ(stats.y_max, 400.0);

  // Check chip counts [2 from chip 0, 1 from chip 1, 1 from chip 2, 1 from chip
  // 3]
  EXPECT_EQ(stats.chip_counts[0], 2);
  EXPECT_EQ(stats.chip_counts[1], 1);
  EXPECT_EQ(stats.chip_counts[2], 1);
  EXPECT_EQ(stats.chip_counts[3], 1);
}

// Test 10: NeutronUtils should filter by ROI correctly
TEST_F(TDCNeutronTest, NeutronUtilsFiltersROICorrectly) {
  // Filter for neutrons with x=[100, 200] and y=[200, 300]
  auto filtered =
      NeutronUtils::filterByROI(test_neutrons, 100.0, 200.0, 200.0, 300.0);

  // Should include:
  // - neutron 0: (100.5, 200.5) ✓
  // - neutron 1: (150.25, 250.75) ✓
  // - neutron 4: (200.0, 300.0) ✓
  // Should exclude:
  // - neutron 2: (300.0, 400.0) ✗ (x and y too high)
  // - neutron 3: (50.1, 75.9) ✗ (x and y too low)

  EXPECT_EQ(filtered.size(), 3);
  EXPECT_DOUBLE_EQ(filtered[0].x, 100.5);
  EXPECT_DOUBLE_EQ(filtered[1].x, 150.25);
  EXPECT_DOUBLE_EQ(filtered[2].x, 200.0);
}

// Test 11: NeutronUtils should filter by TOF correctly
TEST_F(TDCNeutronTest, NeutronUtilsFiltersTOFCorrectly) {
  // Filter for TOF range [1500, 3000]
  auto filtered = NeutronUtils::filterByTOF(test_neutrons, 1500, 3000);

  // Should include neutrons with TOF: 2000, 3000, 1500
  // Should exclude: 1000, 4000
  EXPECT_EQ(filtered.size(), 3);

  std::vector<uint32_t> tofs;
  for (const auto& n : filtered) {
    tofs.push_back(n.tof);
  }
  std::sort(tofs.begin(), tofs.end());

  EXPECT_EQ(tofs[0], 1500);
  EXPECT_EQ(tofs[1], 2000);
  EXPECT_EQ(tofs[2], 3000);
}

// Test 12: NeutronUtils should filter by cluster size correctly
TEST_F(TDCNeutronTest, NeutronUtilsFiltersClusterSizeCorrectly) {
  // Filter for cluster sizes [2, 8]
  auto filtered = NeutronUtils::filterByClusterSize(test_neutrons, 2, 8);

  // Should include neutrons with n_hits: 4, 8, 2
  // Should exclude: 1, 15
  EXPECT_EQ(filtered.size(), 3);

  std::vector<uint16_t> cluster_sizes;
  for (const auto& n : filtered) {
    cluster_sizes.push_back(n.n_hits);
  }
  std::sort(cluster_sizes.begin(), cluster_sizes.end());

  EXPECT_EQ(cluster_sizes[0], 2);
  EXPECT_EQ(cluster_sizes[1], 4);
  EXPECT_EQ(cluster_sizes[2], 8);
}

// Test 13: NeutronUtils should filter by chip correctly
TEST_F(TDCNeutronTest, NeutronUtilsFiltersChipCorrectly) {
  auto filtered = NeutronUtils::filterByChip(test_neutrons, 0);

  // Should include 2 neutrons from chip 0
  EXPECT_EQ(filtered.size(), 2);
  for (const auto& n : filtered) {
    EXPECT_EQ(n.chip_id, 0);
  }
}

// Test 14: NeutronUtils should create 2D histogram correctly
TEST_F(TDCNeutronTest, NeutronUtilsCreates2DHistogramCorrectly) {
  // Create 2x2 histogram with manual ranges
  auto histogram = NeutronUtils::create2DHistogram(
      test_neutrons, 2, 2, std::make_pair(0.0, 400.0),  // x range
      std::make_pair(0.0, 400.0)                        // y range
  );

  EXPECT_EQ(histogram.size(), 2);     // 2 x bins
  EXPECT_EQ(histogram[0].size(), 2);  // 2 y bins
  EXPECT_EQ(histogram[1].size(), 2);

  // Check that total counts match number of neutrons
  size_t total_counts = 0;
  for (const auto& row : histogram) {
    for (auto count : row) {
      total_counts += count;
    }
  }
  EXPECT_EQ(total_counts, test_neutrons.size());
}

// Test 15: NeutronUtils should create TOF spectrum correctly
TEST_F(TDCNeutronTest, NeutronUtilsCreatesTOFSpectrumCorrectly) {
  auto [bin_centers, counts] =
      NeutronUtils::createTOFSpectrum(test_neutrons, 4);

  EXPECT_EQ(bin_centers.size(), 4);
  EXPECT_EQ(counts.size(), 4);

  // Check that total counts match number of neutrons
  size_t total_counts = 0;
  for (auto count : counts) {
    total_counts += count;
  }
  EXPECT_EQ(total_counts, test_neutrons.size());
}

// Test 16: NeutronUtils should create cluster size distribution correctly
TEST_F(TDCNeutronTest, NeutronUtilsCreatesClusterSizeDistributionCorrectly) {
  auto [sizes, counts] =
      NeutronUtils::createClusterSizeDistribution(test_neutrons);

  // Should have entries for cluster sizes: 1, 2, 4, 8, 15
  EXPECT_GE(sizes.size(), 5);

  // Check that total counts match number of neutrons
  size_t total_counts = 0;
  for (auto count : counts) {
    total_counts += count;
  }
  EXPECT_EQ(total_counts, test_neutrons.size());
}

// Test 17: NeutronUtils should convert to hits correctly
TEST_F(TDCNeutronTest, NeutronUtilsConvertsToHitsCorrectly) {
  auto hits = NeutronUtils::convertToHits(test_neutrons);

  EXPECT_EQ(hits.size(), test_neutrons.size());

  for (size_t i = 0; i < hits.size(); ++i) {
    // Coordinates should be rounded to nearest integer
    EXPECT_EQ(hits[i].x, static_cast<uint16_t>(std::round(test_neutrons[i].x)));
    EXPECT_EQ(hits[i].y, static_cast<uint16_t>(std::round(test_neutrons[i].y)));
    EXPECT_EQ(hits[i].tof, test_neutrons[i].tof);
    EXPECT_EQ(hits[i].tot, test_neutrons[i].tot);
    EXPECT_EQ(hits[i].chip_id, test_neutrons[i].chip_id);
  }
}

// Test 18: NeutronUtils should sort by TOF correctly
TEST_F(TDCNeutronTest, NeutronUtilsSortsByTOFCorrectly) {
  auto neutrons_copy = test_neutrons;

  // Sort ascending
  NeutronUtils::sortByTOF(neutrons_copy, true);
  EXPECT_EQ(neutrons_copy[0].tof, 1000);
  EXPECT_EQ(neutrons_copy[1].tof, 1500);
  EXPECT_EQ(neutrons_copy[2].tof, 2000);
  EXPECT_EQ(neutrons_copy[3].tof, 3000);
  EXPECT_EQ(neutrons_copy[4].tof, 4000);

  // Sort descending
  NeutronUtils::sortByTOF(neutrons_copy, false);
  EXPECT_EQ(neutrons_copy[0].tof, 4000);
  EXPECT_EQ(neutrons_copy[1].tof, 3000);
  EXPECT_EQ(neutrons_copy[2].tof, 2000);
  EXPECT_EQ(neutrons_copy[3].tof, 1500);
  EXPECT_EQ(neutrons_copy[4].tof, 1000);
}

// Test 19: NeutronUtils should sort by cluster size correctly
TEST_F(TDCNeutronTest, NeutronUtilsSortsByClusterSizeCorrectly) {
  auto neutrons_copy = test_neutrons;

  // Sort descending (default - largest first)
  NeutronUtils::sortByClusterSize(neutrons_copy, false);
  EXPECT_EQ(neutrons_copy[0].n_hits, 15);
  EXPECT_EQ(neutrons_copy[1].n_hits, 8);
  EXPECT_EQ(neutrons_copy[2].n_hits, 4);
  EXPECT_EQ(neutrons_copy[3].n_hits, 2);
  EXPECT_EQ(neutrons_copy[4].n_hits, 1);

  // Sort ascending
  NeutronUtils::sortByClusterSize(neutrons_copy, true);
  EXPECT_EQ(neutrons_copy[0].n_hits, 1);
  EXPECT_EQ(neutrons_copy[1].n_hits, 2);
  EXPECT_EQ(neutrons_copy[2].n_hits, 4);
  EXPECT_EQ(neutrons_copy[3].n_hits, 8);
  EXPECT_EQ(neutrons_copy[4].n_hits, 15);
}

// Test 20: NeutronUtils should validate data correctly
TEST_F(TDCNeutronTest, NeutronUtilsValidatesDataCorrectly) {
  // Valid data should pass validation
  EXPECT_TRUE(NeutronUtils::validateData(test_neutrons));

  // Create invalid data
  std::vector<TDCNeutron> invalid_neutrons = {
      TDCNeutron(100.0, 200.0, 1000, 50, 0, 0),  // Invalid: 0 hits
      TDCNeutron(-10.0, 200.0, 1000, 50, 1, 0),  // Invalid: negative coordinate
      TDCNeutron(100.0, 200.0, 1000, 50, 1, 5),  // Invalid: chip_id > 3
  };

  EXPECT_FALSE(NeutronUtils::validateData(invalid_neutrons));

  // Empty vector should be valid
  std::vector<TDCNeutron> empty_neutrons;
  EXPECT_TRUE(NeutronUtils::validateData(empty_neutrons));
}

}  // namespace tdcsophiread