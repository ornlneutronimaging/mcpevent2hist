// TDCSophiread Cluster Processor Tests
// TDD approach: Tests for complete hits-to-neutrons processing pipeline

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <vector>

#include "tdc_cluster_processor.h"
#include "tdc_clustering_config.h"

namespace tdcsophiread {

// Test class for TDCClusterProcessor
class TDCClusterProcessorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test configuration with VENUS defaults
    config_ = ClusteringConfig::venusDefaults();

    // Create test hits for processing
    // Cluster 1: Compact 2x2 cluster
    test_hits_ = {
        TDCHit(1000, 100, 100, 1000, 150, 0),  // Bottom-left
        TDCHit(1001, 101, 100, 1001, 180, 0),  // Bottom-right
        TDCHit(1002, 100, 101, 1002, 160, 0),  // Top-left
        TDCHit(1003, 101, 101, 1003, 200, 0),  // Top-right

        // Cluster 2: Single isolated hit
        TDCHit(2000, 200, 200, 2000, 120, 1),

        // Cluster 3: Linear cluster (3 hits)
        TDCHit(3000, 300, 300, 3000, 140, 2),
        TDCHit(3001, 301, 300, 3001, 170, 2),
        TDCHit(3002, 302, 300, 3002, 160, 2),
    };

    // Create large test dataset for performance testing
    large_test_hits_.reserve(10000);
    for (size_t i = 0; i < 10000; ++i) {
      uint16_t x = static_cast<uint16_t>(i % 512);
      uint16_t y = static_cast<uint16_t>(i / 512);
      uint32_t tof = static_cast<uint32_t>(1000 + i);
      uint16_t tot = static_cast<uint16_t>(100 + (i % 100));
      uint8_t chip_id = static_cast<uint8_t>(i % 4);

      large_test_hits_.emplace_back(tof, x, y, tof, tot, chip_id);
    }
  }

  ClusteringConfig config_;
  std::vector<TDCHit> test_hits_;
  std::vector<TDCHit> large_test_hits_;
};

// Test 1: TDCClusterProcessor should construct with default configuration
TEST_F(TDCClusterProcessorTest, ConstructsWithDefaultConfiguration) {
  EXPECT_NO_THROW(TDCClusterProcessor processor);

  TDCClusterProcessor processor;
  EXPECT_EQ(processor.getClusteringAlgorithm(), "abs");
  EXPECT_EQ(processor.getPeakFittingAlgorithm(), "centroid");
  EXPECT_TRUE(processor.isClusteringEnabled());
}

// Test 2: TDCClusterProcessor should construct with custom configuration
TEST_F(TDCClusterProcessorTest, ConstructsWithCustomConfiguration) {
  config_.abs.radius = 3.0;
  config_.centroid.super_resolution_factor = 16.0;

  TDCClusterProcessor processor(config_);

  EXPECT_EQ(processor.getConfiguration().abs.radius, 3.0);
  EXPECT_EQ(processor.getConfiguration().centroid.super_resolution_factor,
            16.0);
}

// Test 3: TDCClusterProcessor should handle empty input gracefully
TEST_F(TDCClusterProcessorTest, HandlesEmptyInputGracefully) {
  TDCClusterProcessor processor(config_);
  std::vector<TDCHit> empty_hits;

  auto neutrons = processor.processHits(empty_hits);

  EXPECT_TRUE(neutrons.empty());
  EXPECT_EQ(processor.getLastNeutronCount(), 0);
  EXPECT_EQ(processor.getLastHitsPerSecond(), 0.0);
}

// Test 4: TDCClusterProcessor should process hits through complete pipeline
TEST_F(TDCClusterProcessorTest, ProcessesHitsThroughCompletePipeline) {
  TDCClusterProcessor processor(config_);

  auto neutrons = processor.processHits(test_hits_);

  // Should produce neutrons from clustering
  EXPECT_GT(neutrons.size(), 0);
  EXPECT_LE(neutrons.size(),
            test_hits_.size());  // Can't have more neutrons than hits

  // Check that neutrons have valid properties
  for (const auto& neutron : neutrons) {
    EXPECT_GT(neutron.n_hits, 0);
    EXPECT_GT(neutron.tot, 0);
    EXPECT_GE(neutron.x, 0.0);
    EXPECT_GE(neutron.y, 0.0);
    EXPECT_LE(neutron.chip_id, 3);
  }

  // Check performance metrics
  EXPECT_EQ(processor.getLastNeutronCount(), neutrons.size());
  EXPECT_GT(processor.getLastProcessingTimeMs(), 0.0);
  EXPECT_GT(processor.getLastHitsPerSecond(), 0.0);
}

// Test 5: TDCClusterProcessor should handle clustering disabled
TEST_F(TDCClusterProcessorTest, HandlesClusteringDisabled) {
  config_.enable_clustering = false;
  TDCClusterProcessor processor(config_);

  auto neutrons = processor.processHits(test_hits_);

  // When clustering is disabled, should get one neutron per hit
  EXPECT_EQ(neutrons.size(), test_hits_.size());
  EXPECT_FALSE(processor.isClusteringEnabled());

  // Each neutron should have exactly 1 hit
  for (const auto& neutron : neutrons) {
    EXPECT_EQ(neutron.n_hits, 1);
  }
}

// Test 6: TDCClusterProcessor should validate input hits
TEST_F(TDCClusterProcessorTest, ValidatesInputHits) {
  TDCClusterProcessor processor(config_);

  // Create invalid hits
  std::vector<TDCHit> invalid_hits = {
      TDCHit(1000, 5000, 100, 1000, 100, 0),  // Invalid x coordinate
      TDCHit(1001, 100, 5000, 1001, 100, 0),  // Invalid y coordinate
      TDCHit(1002, 100, 100, 1002, 100, 5),   // Invalid chip_id
  };

  EXPECT_THROW(processor.processHits(invalid_hits), std::invalid_argument);
}

// Test 7: TDCClusterProcessor should process hits with progress callback
TEST_F(TDCClusterProcessorTest, ProcessesHitsWithProgressCallback) {
  TDCClusterProcessor processor(config_);

  std::vector<double> progress_values;
  auto progress_callback = [&progress_values](double progress) {
    progress_values.push_back(progress);
  };

  auto neutrons =
      processor.processHitsWithProgress(test_hits_, progress_callback);

  EXPECT_GT(neutrons.size(), 0);
  EXPECT_GE(progress_values.size(), 2);  // Should have at least start and end
  EXPECT_EQ(progress_values.front(), 0.0);  // Should start at 0
  EXPECT_EQ(progress_values.back(), 1.0);   // Should end at 1

  // Progress should be non-decreasing
  for (size_t i = 1; i < progress_values.size(); ++i) {
    EXPECT_GE(progress_values[i], progress_values[i - 1]);
  }
}

// Test 8: TDCClusterProcessor should process hits in chunks
TEST_F(TDCClusterProcessorTest, ProcessesHitsInChunks) {
  TDCClusterProcessor processor(config_);

  // Process with small chunk size
  auto neutrons_chunked = processor.processHitsInChunks(test_hits_, 3);

  // Reset and process all at once for comparison
  processor.reset();
  auto neutrons_all = processor.processHits(test_hits_);

  // Should produce similar results (may differ due to chunk boundaries)
  EXPECT_GT(neutrons_chunked.size(), 0);
  EXPECT_GT(neutrons_all.size(), 0);
}

// Test 9: TDCClusterProcessor should process hits by chip
TEST_F(TDCClusterProcessorTest, ProcessesHitsByChip) {
  TDCClusterProcessor processor(config_);

  auto chip_neutrons = processor.processHitsByChip(test_hits_);

  // Should have results for chips that have hits
  EXPECT_GT(chip_neutrons.size(), 0);

  // All neutrons should have correct chip assignments
  for (const auto& [chip_id, neutrons] : chip_neutrons) {
    for (const auto& neutron : neutrons) {
      EXPECT_EQ(neutron.chip_id, chip_id);
    }
  }
}

// Test 10: TDCClusterProcessor should update configuration correctly
TEST_F(TDCClusterProcessorTest, UpdatesConfigurationCorrectly) {
  TDCClusterProcessor processor(config_);

  // Update configuration
  ClusteringConfig new_config = config_;
  new_config.abs.radius = 7.0;
  new_config.centroid.super_resolution_factor = 4.0;

  processor.configure(new_config);

  EXPECT_EQ(processor.getConfiguration().abs.radius, 7.0);
  EXPECT_EQ(processor.getConfiguration().centroid.super_resolution_factor, 4.0);
}

// Test 11: TDCClusterProcessor should reset state correctly
TEST_F(TDCClusterProcessorTest, ResetsStateCorrectly) {
  TDCClusterProcessor processor(config_);

  // Process some hits to generate state
  processor.processHits(test_hits_);

  EXPECT_GT(processor.getLastNeutronCount(), 0);
  EXPECT_GT(processor.getLastProcessingTimeMs(), 0.0);

  // Reset and verify
  processor.reset();

  EXPECT_EQ(processor.getLastNeutronCount(), 0);
  EXPECT_EQ(processor.getLastProcessingTimeMs(), 0.0);
}

// Test 12: TDCClusterProcessor should provide algorithm access
TEST_F(TDCClusterProcessorTest, ProvidesAlgorithmAccess) {
  TDCClusterProcessor processor(config_);

  // Should provide access to underlying algorithms
  EXPECT_NE(processor.getClusteringAlgorithmPtr(), nullptr);
  EXPECT_NE(processor.getPeakFittingAlgorithmPtr(), nullptr);

  // Algorithm names should match configuration
  EXPECT_EQ(processor.getClusteringAlgorithm(), config_.clustering_algorithm);
  EXPECT_EQ(processor.getPeakFittingAlgorithm(),
            config_.peak_fitting_algorithm);
}

// Test 13: TDCClusterProcessor should generate processing summary
TEST_F(TDCClusterProcessorTest, GeneratesProcessingSummary) {
  TDCClusterProcessor processor(config_);

  processor.processHits(test_hits_);

  std::string summary = processor.getProcessingSummary();

  // Summary should contain key information
  EXPECT_TRUE(summary.find("TDC Cluster Processing Summary") !=
              std::string::npos);
  EXPECT_TRUE(summary.find("Clustering Algorithm: abs") != std::string::npos);
  EXPECT_TRUE(summary.find("Peak Fitting Algorithm: centroid") !=
              std::string::npos);
  EXPECT_TRUE(summary.find("Input Hits:") != std::string::npos);
  EXPECT_TRUE(summary.find("Output Neutrons:") != std::string::npos);
}

// Test 14: TDCClusterProcessor should handle large datasets efficiently
TEST_F(TDCClusterProcessorTest, HandlesLargeDatasetsEfficiently) {
  TDCClusterProcessor processor(config_);

  auto start_time = std::chrono::high_resolution_clock::now();
  auto neutrons = processor.processHits(large_test_hits_);
  auto end_time = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  EXPECT_GT(neutrons.size(), 0);
  EXPECT_LT(duration.count(), 5000);  // Should complete within 5 seconds
  EXPECT_GT(processor.getLastHitsPerSecond(),
            1000.0);  // Should achieve reasonable throughput
}

// Test 15: TDCClusterProcessor should calculate neutron efficiency correctly
TEST_F(TDCClusterProcessorTest, CalculatesNeutronEfficiencyCorrectly) {
  TDCClusterProcessor processor(config_);

  auto neutrons = processor.processHits(test_hits_);

  double efficiency = processor.getLastNeutronEfficiency();

  EXPECT_GE(efficiency, 0.0);
  EXPECT_LE(efficiency, 1.0);

  // Efficiency should equal neutrons/hits
  double expected_efficiency = static_cast<double>(neutrons.size()) /
                               static_cast<double>(test_hits_.size());
  EXPECT_DOUBLE_EQ(efficiency, expected_efficiency);
}

// Utility function tests

// Test 16: ClusterProcessingUtils should filter hits by chip correctly
TEST_F(TDCClusterProcessorTest, UtilsFiltersHitsByChipCorrectly) {
  auto chip0_hits = ClusterProcessingUtils::filterHitsByChip(test_hits_, 0);
  auto chip1_hits = ClusterProcessingUtils::filterHitsByChip(test_hits_, 1);
  auto chip2_hits = ClusterProcessingUtils::filterHitsByChip(test_hits_, 2);

  // Verify all hits have correct chip ID
  for (const auto& hit : chip0_hits) {
    EXPECT_EQ(hit.chip_id, 0);
  }
  for (const auto& hit : chip1_hits) {
    EXPECT_EQ(hit.chip_id, 1);
  }
  for (const auto& hit : chip2_hits) {
    EXPECT_EQ(hit.chip_id, 2);
  }

  // Total should equal original
  EXPECT_EQ(chip0_hits.size() + chip1_hits.size() + chip2_hits.size(),
            test_hits_.size());
}

// Test 17: ClusterProcessingUtils should sort hits by timestamp correctly
TEST_F(TDCClusterProcessorTest, UtilsSortsHitsByTimestampCorrectly) {
  std::vector<TDCHit> unsorted_hits = test_hits_;

  // Shuffle the hits to ensure they're not sorted
  std::random_device rd;
  std::mt19937 gen(rd());
  std::shuffle(unsorted_hits.begin(), unsorted_hits.end(), gen);

  // Verify hits are not initially sorted (after shuffling)
  bool is_sorted = std::is_sorted(
      unsorted_hits.begin(), unsorted_hits.end(),
      [](const TDCHit& a, const TDCHit& b) { return a.tof < b.tof; });
  // Note: There's a small chance they're still sorted after shuffling, but very
  // unlikely

  ClusterProcessingUtils::sortHitsByTimestamp(unsorted_hits);

  // Verify hits are now sorted
  is_sorted = std::is_sorted(
      unsorted_hits.begin(), unsorted_hits.end(),
      [](const TDCHit& a, const TDCHit& b) { return a.tof < b.tof; });
  EXPECT_TRUE(is_sorted);
}

// Test 18: ClusterProcessingUtils should validate cluster labels correctly
TEST_F(TDCClusterProcessorTest, UtilsValidatesClusterLabelsCorrectly) {
  // Create hits with valid cluster labels
  std::vector<TDCHit> valid_hits = test_hits_;
  for (size_t i = 0; i < valid_hits.size(); ++i) {
    valid_hits[i].cluster_id = static_cast<int>(i % 3);  // Clusters 0, 1, 2
  }

  EXPECT_TRUE(ClusterProcessingUtils::validateClusterLabels(valid_hits));

  // Create hits with invalid cluster labels (huge gaps)
  std::vector<TDCHit> invalid_hits = test_hits_;
  for (size_t i = 0; i < invalid_hits.size(); ++i) {
    invalid_hits[i].cluster_id = static_cast<int>(i * 1000);  // Huge gaps
  }

  EXPECT_FALSE(ClusterProcessingUtils::validateClusterLabels(invalid_hits));
}

// Test 19: ClusterProcessingUtils should create hit subsets correctly
TEST_F(TDCClusterProcessorTest, UtilsCreatesHitSubsetsCorrectly) {
  size_t subset_size = 5;

  // Test non-random subset
  auto subset =
      ClusterProcessingUtils::createHitSubset(test_hits_, subset_size, false);
  EXPECT_EQ(subset.size(), subset_size);

  // Should be first N hits
  for (size_t i = 0; i < subset_size; ++i) {
    EXPECT_EQ(subset[i].tof, test_hits_[i].tof);
  }

  // Test random subset
  auto random_subset =
      ClusterProcessingUtils::createHitSubset(test_hits_, subset_size, true);
  EXPECT_EQ(random_subset.size(), subset_size);

  // If dataset is small enough, should return all hits
  auto full_subset = ClusterProcessingUtils::createHitSubset(
      test_hits_, test_hits_.size() + 10, false);
  EXPECT_EQ(full_subset.size(), test_hits_.size());
}

// Test 20: ClusterProcessingUtils should estimate memory usage reasonably
TEST_F(TDCClusterProcessorTest, UtilsEstimatesMemoryUsageReasonably) {
  size_t memory_usage =
      ClusterProcessingUtils::estimateMemoryUsage(1000, config_);

  // Should be reasonable (greater than just the hit size but not excessive)
  size_t min_expected = 1000 * sizeof(TDCHit);
  size_t max_expected = min_expected * 10;  // Allow some overhead

  EXPECT_GE(memory_usage, min_expected);
  EXPECT_LE(memory_usage, max_expected);

  // Larger hit counts should require more memory
  size_t larger_memory =
      ClusterProcessingUtils::estimateMemoryUsage(10000, config_);
  EXPECT_GT(larger_memory, memory_usage);
}

}  // namespace tdcsophiread