// TDCSophiread Basic Neutron Processor Integration Tests
// End-to-end testing of complete hits → neutrons pipeline

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

#include "neutron_processing/basic_neutron_processor.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Test fixture for Basic Neutron Processor integration testing
 */
class BasicNeutronProcessorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test configuration based on VENUS defaults
    config_ = NeutronProcessingConfig::venusDefaults();

    // Create processor instance
    processor_ = std::make_unique<BasicNeutronProcessor>(config_);
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

  // Create realistic test dataset
  std::vector<TDCHit> createRealisticDataset() {
    std::vector<TDCHit> hits;

    // Neutron 1: 3x3 cluster at (100,100) - all within 75ns window
    for (int dx = 0; dx < 3; ++dx) {
      for (int dy = 0; dy < 3; ++dy) {
        uint16_t tot = 100 + (dx + dy) * 20;  // Varying TOT
        // Keep all hits within 75ns (3 TDC units) window
        uint32_t tof =
            1000 + (dx + dy) / 2;  // Max difference will be 2 TDC units = 50ns
        hits.push_back(createHit(100 + dx, 100 + dy, tof, tot));
      }
    }

    // Neutron 2: 2x2 cluster at (200,200) - all within 75ns window
    for (int dx = 0; dx < 2; ++dx) {
      for (int dy = 0; dy < 2; ++dy) {
        uint16_t tot = 150 + (dx + dy) * 25;
        uint32_t tof = 1100 + (dx + dy) / 2;  // Keep within correlation window
        hits.push_back(createHit(200 + dx, 200 + dy, tof, tot));
      }
    }

    // Isolated hits (gamma noise)
    hits.push_back(createHit(50, 50, 900, 80));
    hits.push_back(createHit(300, 300, 1200, 90));
    hits.push_back(createHit(400, 400, 1300, 85));

    // Neutron 3: Small 2-hit cluster at (150,150)
    hits.push_back(createHit(150, 150, 1050, 120));
    hits.push_back(createHit(151, 150, 1051, 130));

    return hits;
  }

  NeutronProcessingConfig config_;
  std::unique_ptr<BasicNeutronProcessor> processor_;
};

// Test 1: Empty input handling
TEST_F(BasicNeutronProcessorTest, EmptyInput) {
  std::vector<TDCHit> empty_hits;

  auto neutrons = processor_->processHits(empty_hits.begin(), empty_hits.end());

  EXPECT_EQ(neutrons.size(), 0);
  EXPECT_EQ(processor_->getLastProcessingTimeMs(), 0.0);
  EXPECT_EQ(processor_->getLastHitsPerSecond(), 0.0);
  EXPECT_EQ(processor_->getLastNeutronEfficiency(), 0.0);
}

// Test 2: Single hit processing
TEST_F(BasicNeutronProcessorTest, SingleHit) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000, 150)};

  auto neutrons = processor_->processHits(hits.begin(), hits.end());

  // Single hit should form neutron (min_cluster_size = 1 by default)
  ASSERT_EQ(neutrons.size(), 1);

  const auto& neutron = neutrons[0];
  EXPECT_EQ(neutron.x, 100.0);
  EXPECT_EQ(neutron.y, 100.0);
  EXPECT_EQ(neutron.tof, 1000);
  EXPECT_EQ(neutron.tot, 150);
  EXPECT_EQ(neutron.n_hits, 1);
}

// Test 3: Two nearby hits (should cluster into one neutron)
TEST_F(BasicNeutronProcessorTest, TwoNearbyHits) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 120),
      createHit(
          102, 101, 1002,
          180)  // Within clustering radius (spatial: 2 pixels, temporal: 50ns)
  };

  auto neutrons = processor_->processHits(hits.begin(), hits.end());

  // Should form single neutron
  ASSERT_EQ(neutrons.size(), 1);

  const auto& neutron = neutrons[0];
  EXPECT_EQ(neutron.n_hits, 2);
  EXPECT_EQ(neutron.tot, 300);   // Combined TOT
  EXPECT_EQ(neutron.tof, 1002);  // TOF from highest TOT hit (180 > 120)

  // Weighted centroid: (100*120 + 102*180)/(120+180) ≈ 101.2
  EXPECT_NEAR(neutron.x, 101.2, 0.1);
  EXPECT_NEAR(neutron.y, 100.6, 0.1);
}

// Test 4: Two distant hits (should form separate neutrons)
TEST_F(BasicNeutronProcessorTest, TwoDistantHits) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000, 120),
      createHit(200, 200, 1100, 180)  // Far apart
  };

  auto neutrons = processor_->processHits(hits.begin(), hits.end());

  // Should form two separate neutrons
  EXPECT_EQ(neutrons.size(), 2);

  // Verify both neutrons
  for (const auto& neutron : neutrons) {
    EXPECT_EQ(neutron.n_hits, 1);
    EXPECT_GT(neutron.tot, 0);
  }
}

// Test 5: Realistic dataset processing
TEST_F(BasicNeutronProcessorTest, RealisticDataset) {
  auto hits = createRealisticDataset();

  auto neutrons = processor_->processHits(hits.begin(), hits.end());

  // Should detect 3 neutrons from the clusters + potentially isolated hits
  EXPECT_GE(neutrons.size(), 3);
  EXPECT_LE(neutrons.size(),
            hits.size());  // Can't have more neutrons than hits

  // Find the 3x3 cluster neutron (largest)
  auto largest_neutron =
      std::max_element(neutrons.begin(), neutrons.end(),
                       [](const TDCNeutron& a, const TDCNeutron& b) {
                         return a.n_hits < b.n_hits;
                       });

  ASSERT_NE(largest_neutron, neutrons.end());
  EXPECT_EQ(largest_neutron->n_hits, 9);  // 3x3 cluster

  // Verify neutron properties are reasonable
  for (const auto& neutron : neutrons) {
    EXPECT_GT(neutron.n_hits, 0);
    EXPECT_GT(neutron.tot, 0);
    EXPECT_GE(neutron.x, 0.0);
    EXPECT_GE(neutron.y, 0.0);
  }
}

// Test 6: Performance measurement
TEST_F(BasicNeutronProcessorTest, PerformanceMeasurement) {
  auto hits = createRealisticDataset();

  auto start_time = std::chrono::high_resolution_clock::now();
  auto neutrons = processor_->processHits(hits.begin(), hits.end());
  auto end_time = std::chrono::high_resolution_clock::now();

  auto manual_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                             end_time - start_time)
                             .count() /
                         1000.0;

  // Verify performance metrics are tracked
  EXPECT_GT(processor_->getLastProcessingTimeMs(), 0.0);
  EXPECT_GT(processor_->getLastHitsPerSecond(), 0.0);

  // Processing time should be reasonable (within order of magnitude)
  EXPECT_LT(processor_->getLastProcessingTimeMs(), manual_duration * 10);

  // Neutron efficiency should be reasonable (0-1 range)
  EXPECT_GE(processor_->getLastNeutronEfficiency(), 0.0);
  EXPECT_LE(processor_->getLastNeutronEfficiency(), 1.0);
}

// Test 7: Configuration management
TEST_F(BasicNeutronProcessorTest, ConfigurationManagement) {
  // Verify initial configuration
  const auto& initial_config = processor_->getConfig();
  EXPECT_EQ(initial_config.clustering.algorithm, "abs");
  EXPECT_EQ(initial_config.extraction.algorithm, "simple_centroid");

  // Test algorithm name reporting
  EXPECT_EQ(processor_->getHitClusteringAlgorithm(), "abs");
  EXPECT_EQ(processor_->getNeutronExtractionAlgorithm(), "simple_centroid");

  // Test configuration update
  NeutronProcessingConfig new_config = config_;
  new_config.clustering.abs.radius = 10.0;
  new_config.extraction.min_tot_threshold = 50;

  processor_->configure(new_config);

  const auto& updated_config = processor_->getConfig();
  EXPECT_EQ(updated_config.clustering.abs.radius, 10.0);
  EXPECT_EQ(updated_config.extraction.min_tot_threshold, 50);
}

// Test 8: Processing with labels (diagnostics mode)
TEST_F(BasicNeutronProcessorTest, ProcessingWithLabels) {
  auto hits = createRealisticDataset();

  auto result = processor_->processHitsWithLabels(hits.begin(), hits.end());

  // Verify we get both neutrons and labels
  EXPECT_GT(result.neutrons.size(), 0);
  EXPECT_EQ(result.cluster_labels.size(), hits.size());

  // Labels should correspond to hits
  for (size_t i = 0; i < result.cluster_labels.size(); ++i) {
    // Labels should be either -1 (unclustered) or >= 0 (cluster ID)
    EXPECT_GE(result.cluster_labels[i], -1);
  }

  // Count clustered hits
  size_t clustered_hits =
      std::count_if(result.cluster_labels.begin(), result.cluster_labels.end(),
                    [](int label) { return label >= 0; });

  // Should have some clustered hits
  EXPECT_GT(clustered_hits, 0);
  EXPECT_LE(clustered_hits, hits.size());
}

// Test 9: Reset functionality
TEST_F(BasicNeutronProcessorTest, ResetFunctionality) {
  auto hits = createRealisticDataset();

  // Process hits
  auto neutrons1 = processor_->processHits(hits.begin(), hits.end());
  EXPECT_GT(processor_->getLastProcessingTimeMs(), 0.0);

  // Reset
  processor_->reset();

  // Process same hits again - should get same result
  auto neutrons2 = processor_->processHits(hits.begin(), hits.end());

  EXPECT_EQ(neutrons1.size(), neutrons2.size());

  // Verify detailed neutron properties match
  ASSERT_EQ(neutrons1.size(), neutrons2.size());

  // Sort neutrons by position for comparison
  auto sort_by_position = [](const TDCNeutron& a, const TDCNeutron& b) {
    return std::tie(a.x, a.y) < std::tie(b.x, b.y);
  };

  std::sort(neutrons1.begin(), neutrons1.end(), sort_by_position);
  std::sort(neutrons2.begin(), neutrons2.end(), sort_by_position);

  for (size_t i = 0; i < neutrons1.size(); ++i) {
    EXPECT_NEAR(neutrons1[i].x, neutrons2[i].x, 1e-6);
    EXPECT_NEAR(neutrons1[i].y, neutrons2[i].y, 1e-6);
    EXPECT_EQ(neutrons1[i].n_hits, neutrons2[i].n_hits);
    EXPECT_EQ(neutrons1[i].tot, neutrons2[i].tot);
  }
}

// Test 10: Large dataset processing
TEST_F(BasicNeutronProcessorTest, LargeDatasetProcessing) {
  std::vector<TDCHit> large_hits;

  // Create 1000 hits in various clusters
  for (int cluster = 0; cluster < 100; ++cluster) {
    int base_x = (cluster % 10) * 50;
    int base_y = (cluster / 10) * 50;
    uint32_t base_tof = 1000 + cluster * 10;

    // Each cluster has 10 hits in a small area, all within 75ns window
    for (int hit = 0; hit < 10; ++hit) {
      int dx = hit % 3;
      int dy = hit / 3;
      // Keep temporal spread within 75ns (3 TDC units)
      uint32_t tof = base_tof + hit / 4;  // Max spread = 2 TDC units = 50ns
      large_hits.push_back(
          createHit(base_x + dx, base_y + dy, tof, 100 + hit * 10));
    }
  }

  auto neutrons = processor_->processHits(large_hits.begin(), large_hits.end());

  // Should process successfully
  EXPECT_GT(neutrons.size(), 50);   // Should find most clusters
  EXPECT_LE(neutrons.size(), 100);  // But not more than created

  // Performance should be reasonable
  EXPECT_GT(processor_->getLastHitsPerSecond(),
            10000);  // At least 10K hits/sec

  // All neutrons should be valid
  for (const auto& neutron : neutrons) {
    EXPECT_GT(neutron.n_hits, 0);
    EXPECT_GT(neutron.tot, 0);
    EXPECT_GE(neutron.x, 0.0);
    EXPECT_GE(neutron.y, 0.0);
  }
}

// Test 11: Statistics validation
TEST_F(BasicNeutronProcessorTest, StatisticsValidation) {
  auto hits = createRealisticDataset();

  auto neutrons = processor_->processHits(hits.begin(), hits.end());
  auto stats = processor_->getStatistics();

  // Verify statistics consistency
  EXPECT_EQ(stats.total_hits_processed, hits.size());
  EXPECT_EQ(stats.total_neutrons_produced, neutrons.size());
  EXPECT_GT(stats.total_processing_time_ms, 0.0);

  // Single-threaded implementation specific
  EXPECT_EQ(stats.num_workers_used, 1);
  EXPECT_EQ(stats.num_batches_created, 1);
  EXPECT_EQ(stats.parallel_efficiency, 1.0);
  EXPECT_EQ(stats.load_balance_factor, 1.0);

  // Memory usage should be reasonable
  EXPECT_GT(stats.peak_memory_usage_mb, 0.0);
  EXPECT_LT(stats.peak_memory_usage_mb,
            100.0);  // Should be much less than 100MB for test data

  // Quality metrics
  if (hits.size() > 0) {
    EXPECT_GE(stats.neutron_efficiency, 0.0);
    EXPECT_LE(stats.neutron_efficiency, 1.0);
  }

  if (stats.total_clusters_found > 0) {
    EXPECT_GT(stats.mean_cluster_size, 0.0);
  }
}

// Test 12: Direct algorithm access (for testing)
TEST_F(BasicNeutronProcessorTest, DirectAlgorithmAccess) {
  // Test access to underlying algorithms
  auto* clusterer = processor_->getClusterer();
  auto* extractor = processor_->getExtractor();

  ASSERT_NE(clusterer, nullptr);
  ASSERT_NE(extractor, nullptr);

  EXPECT_EQ(clusterer->getName(), "abs");
  EXPECT_EQ(extractor->getName(), "simple_centroid");
}

}  // namespace tdcsophiread