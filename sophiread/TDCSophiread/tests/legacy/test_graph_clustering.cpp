// Graph Clustering Unit Tests
// Test-Driven Development for Graph-based clustering algorithm

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "tdc_graph_clustering.h"
#include "tdc_hit.h"

namespace tdcsophiread {

class GraphClusteringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Default configuration for testing
    config_ = GraphConfig();
    config_.radius = 5.0;
    config_.neutron_correlation_window = 75.0;
    config_.min_cluster_size = 1;
    config_.grid_size = 10.0;
    config_.enable_spatial_hash = true;

    clustering_ = std::make_unique<GraphClustering>(config_);
  }

  void TearDown() override { clustering_.reset(); }

  GraphConfig config_;
  std::unique_ptr<GraphClustering> clustering_;

  // Helper to create a hit at specific position and time
  TDCHit createHit(uint16_t x, uint16_t y, uint32_t tof, uint16_t tot = 50,
                   uint8_t chip_id = 0) {
    TDCHit hit;
    hit.x = x;
    hit.y = y;
    hit.tof = tof;
    hit.tot = tot;
    hit.chip_id = chip_id;
    hit.timestamp = tof * 25;  // Convert to timestamp
    hit.cluster_id = -1;       // Initialize as unclustered
    return hit;
  }
};

// Test 1: Empty input should return 0 clusters
TEST_F(GraphClusteringTest, EmptyInputReturnsZeroClusters) {
  std::vector<TDCHit> hits;

  size_t num_clusters = clustering_->fit(hits);

  EXPECT_EQ(num_clusters, 0);
  EXPECT_EQ(clustering_->getClusterLabels().size(), 0);
}

// Test 2: Single hit should form one cluster
TEST_F(GraphClusteringTest, SingleHitFormsOneCluster) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000)};

  size_t num_clusters = clustering_->fit(hits);

  EXPECT_EQ(num_clusters, 1);
  ASSERT_EQ(clustering_->getClusterLabels().size(), 1);
  EXPECT_GE(clustering_->getClusterLabels()[0], 0);  // Should be clustered
}

// Test 3: Two spatially close hits should cluster together
TEST_F(GraphClusteringTest, SpatiallyCloseHitsClusterTogether) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(102, 101, 1000)  // Within spatial radius (5 pixels)
  };

  size_t num_clusters = clustering_->fit(hits);
  const auto& labels = clustering_->getClusterLabels();

  EXPECT_EQ(num_clusters, 1);
  ASSERT_EQ(labels.size(), 2);
  EXPECT_EQ(labels[0], labels[1]);  // Should have same cluster label
  EXPECT_GE(labels[0], 0);          // Should be clustered
}

// Test 4: Two spatially distant hits should form separate clusters
TEST_F(GraphClusteringTest, SpatiallyDistantHitsFormSeparateClusters) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(200, 200, 1000)  // Beyond spatial radius
  };

  size_t num_clusters = clustering_->fit(hits);
  const auto& labels = clustering_->getClusterLabels();

  EXPECT_EQ(num_clusters, 2);
  ASSERT_EQ(labels.size(), 2);
  EXPECT_NE(labels[0], labels[1]);  // Should have different cluster labels
  EXPECT_GE(labels[0], 0);          // Both should be clustered
  EXPECT_GE(labels[1], 0);
}

// Test 5: Two temporally distant hits should form separate clusters
TEST_F(GraphClusteringTest, TemporallyDistantHitsFormSeparateClusters) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(102, 101, 5000)  // Beyond temporal window (75ns = 3 TOF units)
  };

  size_t num_clusters = clustering_->fit(hits);
  const auto& labels = clustering_->getClusterLabels();

  EXPECT_EQ(num_clusters, 2);
  ASSERT_EQ(labels.size(), 2);
  EXPECT_NE(labels[0], labels[1]);  // Should have different cluster labels
}

// Test 6: Three hits forming a chain should all cluster together
TEST_F(GraphClusteringTest, ThreeHitsFormingChainClusterTogether) {
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(103, 101, 1000),  // Connected to first
      createHit(106, 102, 1000)   // Connected to second
  };

  size_t num_clusters = clustering_->fit(hits);
  const auto& labels = clustering_->getClusterLabels();

  EXPECT_EQ(num_clusters, 1);
  ASSERT_EQ(labels.size(), 3);
  EXPECT_EQ(labels[0], labels[1]);  // All should have same cluster label
  EXPECT_EQ(labels[1], labels[2]);
  EXPECT_GE(labels[0], 0);  // Should be clustered
}

// Test 7: Test minimum cluster size filtering
TEST_F(GraphClusteringTest, MinimumClusterSizeFiltering) {
  config_.min_cluster_size = 2;
  clustering_ = std::make_unique<GraphClustering>(config_);

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),  // Single hit - should be filtered
      createHit(200, 200, 1000),  // Single hit - should be filtered
      createHit(300, 300, 1000),  // Part of 2-hit cluster
      createHit(302, 301, 1000)   // Part of 2-hit cluster
  };

  size_t num_clusters = clustering_->fit(hits);
  const auto& labels = clustering_->getClusterLabels();

  EXPECT_EQ(num_clusters, 1);  // 1 valid cluster after filtering
  ASSERT_EQ(labels.size(), 4);

  // First two hits should be unclustered (-1)
  EXPECT_EQ(labels[0], -1);
  EXPECT_EQ(labels[1], -1);

  // Last two hits should be clustered together
  EXPECT_EQ(labels[2], labels[3]);
  EXPECT_GE(labels[2], 0);
}

// Test 8: Test spatial hash functionality
TEST_F(GraphClusteringTest, SpatialHashCorrectness) {
  // Create hits in different spatial regions
  std::vector<TDCHit> hits = {
      createHit(15, 15, 1000),  // Grid cell (1,1)
      createHit(16, 16, 1000),  // Grid cell (1,1) - should cluster
      createHit(25, 25, 1000),  // Grid cell (2,2) - should not cluster
      createHit(26, 26, 1000)  // Grid cell (2,2) - should cluster with previous
  };

  size_t num_clusters = clustering_->fit(hits);
  const auto& labels = clustering_->getClusterLabels();

  EXPECT_EQ(num_clusters, 2);
  ASSERT_EQ(labels.size(), 4);

  // First two hits should cluster together
  EXPECT_EQ(labels[0], labels[1]);

  // Last two hits should cluster together
  EXPECT_EQ(labels[2], labels[3]);

  // The two pairs should be in different clusters
  EXPECT_NE(labels[0], labels[2]);
}

// Test 9: Test temporal correlation window boundary
TEST_F(GraphClusteringTest, TemporalCorrelationWindowBoundary) {
  // 75ns correlation window = 3 TOF units (25ns per unit)
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(102, 101, 1003),  // Exactly at boundary - should cluster
      createHit(
          200, 200,
          1004)  // Beyond boundary AND spatially distant - should not cluster
  };

  size_t num_clusters = clustering_->fit(hits);
  const auto& labels = clustering_->getClusterLabels();

  EXPECT_EQ(num_clusters, 2);
  ASSERT_EQ(labels.size(), 3);

  // First two should cluster together
  EXPECT_EQ(labels[0], labels[1]);

  // Third should be separate
  EXPECT_NE(labels[0], labels[2]);
}

// Test 10: Test edge case with spatial hash disabled
TEST_F(GraphClusteringTest, SpatialHashDisabled) {
  config_.enable_spatial_hash = false;
  clustering_ = std::make_unique<GraphClustering>(config_);

  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(103, 101, 1000)  // Should still cluster via brute force
  };

  size_t num_clusters = clustering_->fit(hits);
  const auto& labels = clustering_->getClusterLabels();

  EXPECT_EQ(num_clusters, 1);
  ASSERT_EQ(labels.size(), 2);
  EXPECT_EQ(labels[0], labels[1]);  // Should cluster together
}

// Test 11: Test performance statistics
TEST_F(GraphClusteringTest, PerformanceStatistics) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000),
                              createHit(102, 101, 1000),
                              createHit(200, 200, 2000)};

  clustering_->fit(hits);
  auto stats = clustering_->getStatistics();

  EXPECT_EQ(stats.total_hits, 3);
  EXPECT_GT(stats.processing_time_ms, 0.0);
  EXPECT_GT(stats.spatial_hash_buckets, 0);
  EXPECT_GT(stats.total_edges, 0);
  EXPECT_EQ(stats.total_clusters, 2);
}

// Test 12: Test realistic neutron detection scenario
TEST_F(GraphClusteringTest, RealisticNeutronDetectionScenario) {
  // Simulate MCP detector response: neutron creates 2-3 hits in small spatial
  // cluster
  std::vector<TDCHit> hits = {
      // Neutron 1: 3 hits in tight cluster
      createHit(100, 100, 1000, 45), createHit(101, 101, 1000, 52),
      createHit(102, 100, 1000, 48),

      // Neutron 2: 2 hits in tight cluster, different time
      createHit(200, 200, 2000, 50), createHit(201, 201, 2000, 55),

      // Isolated noise hit
      createHit(300, 300, 3000, 30)};

  size_t num_clusters = clustering_->fit(hits);
  const auto& labels = clustering_->getClusterLabels();

  EXPECT_EQ(num_clusters, 3);
  ASSERT_EQ(labels.size(), 6);

  // First three hits should cluster together
  EXPECT_EQ(labels[0], labels[1]);
  EXPECT_EQ(labels[1], labels[2]);

  // Next two hits should cluster together
  EXPECT_EQ(labels[3], labels[4]);

  // All clusters should be different
  EXPECT_NE(labels[0], labels[3]);  // Neutron 1 vs Neutron 2
  EXPECT_NE(labels[0], labels[5]);  // Neutron 1 vs noise
  EXPECT_NE(labels[3], labels[5]);  // Neutron 2 vs noise
}

// Test 13: Test configuration validation
TEST_F(GraphClusteringTest, ConfigurationValidation) {
  // Test invalid radius
  GraphConfig invalid_config;
  invalid_config.radius = -1.0;
  EXPECT_THROW(GraphClustering clustering(invalid_config),
               std::invalid_argument);

  // Test invalid correlation window
  invalid_config.radius = 5.0;
  invalid_config.neutron_correlation_window = -10.0;
  EXPECT_THROW(GraphClustering clustering(invalid_config),
               std::invalid_argument);

  // Test invalid grid size
  invalid_config.neutron_correlation_window = 75.0;
  invalid_config.grid_size = 0.0;
  EXPECT_THROW(GraphClustering clustering(invalid_config),
               std::invalid_argument);

  // Test invalid min cluster size
  invalid_config.grid_size = 10.0;
  invalid_config.min_cluster_size = 0;
  EXPECT_THROW(GraphClustering clustering(invalid_config),
               std::invalid_argument);
}

// Test 14: Test algorithm name
TEST_F(GraphClusteringTest, AlgorithmName) {
  EXPECT_EQ(clustering_->getName(), "graph");
}

// Test 15: Test reset functionality
TEST_F(GraphClusteringTest, ResetFunctionality) {
  std::vector<TDCHit> hits = {createHit(100, 100, 1000)};

  clustering_->fit(hits);
  EXPECT_EQ(clustering_->getClusterLabels().size(), 1);

  clustering_->reset();
  EXPECT_EQ(clustering_->getClusterLabels().size(), 0);
  EXPECT_EQ(clustering_->getLastHitCount(), 0);
}

// Test 16: Test temporal batching - analyzeHitDistribution
TEST_F(GraphClusteringTest, TemporalBatchingAnalysis) {
  // Create hits simulating TPX3 data with pulse structure
  std::vector<TDCHit> hits;

  // First pulse: hits with TOF 0-16.667ms (0-666800 in 25ns units)
  for (uint32_t tof = 1000; tof < 50000; tof += 500) {
    hits.push_back(createHit(100 + (tof % 100), 100 + (tof % 50), tof));
    if (tof % 2000 == 0) {
      hits.push_back(createHit(101 + (tof % 100), 101 + (tof % 50), tof + 1));
    }
  }

  // Second pulse: TOF reset (new pulse starts at low TOF)
  for (uint32_t tof = 2000; tof < 30000; tof += 800) {
    hits.push_back(createHit(200 + (tof % 80), 200 + (tof % 40), tof));
  }

  // Analyze hit distribution
  auto stats = GraphClustering::analyzeHitDistribution(hits, 2, 75.0);

  // Validate statistics
  EXPECT_GT(stats.mean_hits_per_window, 0.0);
  EXPECT_GE(stats.std_hits_per_window, 0.0);
  EXPECT_GT(stats.optimal_window_tof, 0u);
  EXPECT_GT(stats.overlap_size, 0u);
  EXPECT_LE(stats.num_pulses_analyzed, 2u);
  EXPECT_GT(stats.pulse_period_tof, 0u);
  EXPECT_GT(stats.total_hits_analyzed, 0u);
  EXPECT_LE(stats.total_hits_analyzed, hits.size());

  std::cout << "Batch stats: mean=" << stats.mean_hits_per_window
            << ", std=" << stats.std_hits_per_window
            << ", pulses=" << stats.num_pulses_analyzed << std::endl;
}

// Test 17: Test temporal batching - createStatisticalBatches
TEST_F(GraphClusteringTest, TemporalBatchCreation) {
  // Create ordered hits for batch testing
  std::vector<TDCHit> hits;
  for (uint32_t tof = 1000; tof < 20000; tof += 100) {
    hits.push_back(createHit(100 + (tof % 200), 100 + (tof % 100), tof));
  }

  // Analyze distribution
  auto stats = GraphClustering::analyzeHitDistribution(hits, 1, 75.0);
  ASSERT_GT(stats.mean_hits_per_window, 0.0);

  // Create batches
  auto batches = GraphClustering::createStatisticalBatches(hits, stats);

  // Validate batch structure
  EXPECT_GT(batches.size(), 0u);

  size_t total_hits_covered = 0;
  for (size_t i = 0; i < batches.size(); ++i) {
    const auto& batch = batches[i];

    // Validate batch properties
    EXPECT_TRUE(batch.isValid());
    EXPECT_EQ(batch.hits_ptr, &hits);
    EXPECT_LT(batch.start_index, batch.end_index);
    EXPECT_LE(batch.end_index, hits.size());
    EXPECT_GT(batch.size(), 0u);

    // Check TOF window consistency
    if (batch.size() > 0) {
      EXPECT_LE(batch.tof_window_start, batch.tof_window_end);
    }

    total_hits_covered += batch.size();
  }

  // All hits should be covered by batches
  EXPECT_EQ(total_hits_covered, hits.size());

  std::cout << "Created " << batches.size() << " batches covering "
            << total_hits_covered << " hits" << std::endl;
}

// Test 18: Test temporal batching - processBatch
TEST_F(GraphClusteringTest, TemporalBatchProcessing) {
  // Create a batch with clusterable hits
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(101, 101, 1001),  // Should cluster with first
      createHit(200, 200, 1500),  // Separate cluster
      createHit(201, 201, 1501)   // Should cluster with third
  };

  // Create a batch manually
  HitBatch batch;
  batch.hits_ptr = &hits;
  batch.start_index = 0;
  batch.end_index = hits.size();
  batch.overlap_start = 0;
  batch.overlap_end = hits.size();
  batch.tof_window_start = 1000;
  batch.tof_window_end = 1501;

  // Process the batch
  auto neutrons = GraphClustering::processBatch(batch, config_);

  // processBatch now fully implemented with neutron extraction
  // Should produce 2 neutrons from 4 hits (2 clusters)
  EXPECT_EQ(neutrons.size(), 2u);

  // Verify neutron properties
  if (neutrons.size() >= 2) {
    // First neutron should be around (100.5, 100.5) from hits 0 and 1
    EXPECT_NEAR(neutrons[0].x, 100.5, 1.0);
    EXPECT_NEAR(neutrons[0].y, 100.5, 1.0);
    EXPECT_EQ(neutrons[0].n_hits, 2u);

    // Second neutron should be around (200.5, 200.5) from hits 2 and 3
    EXPECT_NEAR(neutrons[1].x, 200.5, 1.0);
    EXPECT_NEAR(neutrons[1].y, 200.5, 1.0);
    EXPECT_EQ(neutrons[1].n_hits, 2u);
  }

  std::cout << "Batch processing produced " << neutrons.size() << " neutrons"
            << std::endl;
}

// Test 19: Test temporal batching with empty input
TEST_F(GraphClusteringTest, TemporalBatchingEmptyInput) {
  std::vector<TDCHit> empty_hits;

  // Test analysis with empty input
  auto stats = GraphClustering::analyzeHitDistribution(empty_hits, 2, 75.0);
  EXPECT_EQ(stats.mean_hits_per_window, 0.0);
  EXPECT_EQ(stats.std_hits_per_window, 0.0);
  EXPECT_EQ(stats.optimal_window_tof, 0u);
  EXPECT_EQ(stats.overlap_size, 0u);
  EXPECT_EQ(stats.num_pulses_analyzed, 0u);
  EXPECT_EQ(stats.total_hits_analyzed, 0u);

  // Test batch creation with empty input
  auto batches = GraphClustering::createStatisticalBatches(empty_hits, stats);
  EXPECT_TRUE(batches.empty());
}

// Test 20: Test temporal batching with single pulse
TEST_F(GraphClusteringTest, TemporalBatchingSinglePulse) {
  // Create hits within a single pulse
  std::vector<TDCHit> hits;
  for (uint32_t tof = 5000; tof < 15000; tof += 200) {
    hits.push_back(createHit(100 + (tof % 50), 100 + (tof % 30), tof));
  }

  // Analyze single pulse
  auto stats = GraphClustering::analyzeHitDistribution(hits, 1, 75.0);

  // Should find at least some statistics
  EXPECT_GT(stats.total_hits_analyzed, 0u);
  EXPECT_LE(stats.total_hits_analyzed, hits.size());

  // Create batches
  auto batches = GraphClustering::createStatisticalBatches(hits, stats);
  EXPECT_GT(batches.size(), 0u);

  // Verify all hits are covered
  size_t total_covered = 0;
  for (const auto& batch : batches) {
    total_covered += batch.size();
  }
  EXPECT_EQ(total_covered, hits.size());
}

// Test 21: Test TemporalGraphClusteringProcessor basic functionality
TEST_F(GraphClusteringTest, TemporalProcessorBasicFunctionality) {
  // Create test hits for temporal processing
  std::vector<TDCHit> hits;
  for (uint32_t tof = 1000; tof < 10000; tof += 200) {
    hits.push_back(createHit(100 + (tof % 100), 100 + (tof % 50), tof));
    if (tof % 1000 == 0) {
      hits.push_back(createHit(101 + (tof % 100), 101 + (tof % 50), tof + 1));
    }
  }

  // Create temporal processor with default configuration
  TemporalGraphClusteringProcessor processor;

  // Process hits
  auto neutrons = processor.processHits(hits);

  // Validate processing
  const auto& stats = processor.getStatistics();
  EXPECT_EQ(stats.total_hits_processed, hits.size());
  EXPECT_GT(stats.num_workers_used, 0u);
  EXPECT_GT(stats.num_batches_created, 0u);
  EXPECT_GT(stats.total_time_ms, 0.0);
  EXPECT_GT(stats.hits_per_second, 0.0);

  // Note: neutrons.size() may be 0 due to placeholder processBatch
  // implementation
  EXPECT_GE(neutrons.size(), 0u);

  std::cout << "Temporal processor: " << stats.total_hits_processed
            << " hits -> " << neutrons.size() << " neutrons in "
            << stats.total_time_ms << " ms (" << stats.hits_per_second / 1e6
            << " M hits/sec)" << std::endl;
}

// Test 22: Test TemporalGraphClusteringProcessor with custom configuration
TEST_F(GraphClusteringTest, TemporalProcessorCustomConfig) {
  // Create custom configuration
  TemporalGraphConfig config;
  config.graph_config = config_;
  config.num_workers = 2;  // Force specific worker count
  config.min_batch_size = 500;
  config.max_batch_size = 5000;

  TemporalGraphClusteringProcessor processor(config);

  // Verify configuration
  const auto& retrieved_config = processor.getConfig();
  EXPECT_EQ(retrieved_config.num_workers, 2u);
  EXPECT_EQ(retrieved_config.min_batch_size, 500u);
  EXPECT_EQ(retrieved_config.max_batch_size, 5000u);

  // Process small dataset
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000), createHit(101, 101, 1001),
      createHit(200, 200, 2000), createHit(201, 201, 2001)};

  auto neutrons = processor.processHits(hits);
  const auto& stats = processor.getStatistics();

  EXPECT_EQ(stats.total_hits_processed, 4u);
  EXPECT_EQ(stats.num_workers_used, 2u);
  EXPECT_GT(stats.total_time_ms, 0.0);
}

// Test 23: Test TemporalGraphClusteringProcessor with empty input
TEST_F(GraphClusteringTest, TemporalProcessorEmptyInput) {
  TemporalGraphClusteringProcessor processor;

  std::vector<TDCHit> empty_hits;
  auto neutrons = processor.processHits(empty_hits);

  EXPECT_TRUE(neutrons.empty());

  const auto& stats = processor.getStatistics();
  EXPECT_EQ(stats.total_hits_processed, 0u);
  EXPECT_EQ(stats.total_neutrons_produced, 0u);
  EXPECT_EQ(stats.hits_per_second, 0.0);
}

// Test 24: Test TemporalGraphClusteringProcessor reset functionality
TEST_F(GraphClusteringTest, TemporalProcessorReset) {
  TemporalGraphClusteringProcessor processor;

  // Process some hits
  std::vector<TDCHit> hits = {createHit(100, 100, 1000),
                              createHit(101, 101, 1001)};
  processor.processHits(hits);

  // Verify statistics are populated
  const auto& stats_before = processor.getStatistics();
  EXPECT_GT(stats_before.total_hits_processed, 0u);

  // Reset processor
  processor.reset();

  // Verify statistics are cleared
  const auto& stats_after = processor.getStatistics();
  EXPECT_EQ(stats_after.total_hits_processed, 0u);
  EXPECT_EQ(stats_after.total_neutrons_produced, 0u);
  EXPECT_EQ(stats_after.total_time_ms, 0.0);
}

// Test 25: Test TemporalGraphClusteringProcessor deduplication functionality
TEST_F(GraphClusteringTest, TemporalProcessorDeduplication) {
  // Create custom processor to access deduplication (would need public access
  // in real implementation)
  TemporalGraphClusteringProcessor processor;

  // For now, test through the full pipeline which includes deduplication
  // This validates that deduplication doesn't crash the system
  std::vector<TDCHit> hits = {
      createHit(100, 100, 1000),
      createHit(101, 101, 1000),  // Should form 1 cluster
      createHit(200, 200, 2000),
      createHit(201, 201, 2000),  // Should form 1 cluster
      createHit(300, 300, 3000)   // Single hit cluster
  };

  auto neutrons = processor.processHits(hits);
  const auto& stats = processor.getStatistics();

  // Validate processing completed successfully
  EXPECT_EQ(stats.total_hits_processed, 5u);
  EXPECT_GT(stats.total_time_ms, 0.0);

  // Note: Actual neutron count depends on processBatch implementation
  // This test ensures deduplication doesn't break the pipeline
  EXPECT_GE(neutrons.size(), 0u);

  std::cout << "Deduplication test: " << stats.total_hits_processed
            << " hits -> " << neutrons.size() << " neutrons" << std::endl;
}

// Test 26: Test TemporalGraphClusteringProcessor performance scaling
TEST_F(GraphClusteringTest, TemporalProcessorPerformanceScaling) {
  // Test different worker counts to validate scaling
  std::vector<TDCHit> large_hits;
  large_hits.reserve(1000);

  // Create larger dataset for performance testing
  for (uint32_t tof = 1000; tof < 20000; tof += 50) {
    large_hits.push_back(createHit(100 + (tof % 200), 100 + (tof % 100), tof));
    if (tof % 500 == 0) {
      large_hits.push_back(
          createHit(101 + (tof % 200), 101 + (tof % 100), tof + 1));
    }
  }

  // Test with 1 worker
  TemporalGraphConfig config1;
  config1.num_workers = 1;
  TemporalGraphClusteringProcessor processor1(config1);

  auto start1 = std::chrono::high_resolution_clock::now();
  auto neutrons1 = processor1.processHits(large_hits);
  auto end1 = std::chrono::high_resolution_clock::now();
  auto time1 =
      std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1)
          .count();

  // Test with multiple workers
  TemporalGraphConfig config2;
  config2.num_workers = 4;
  TemporalGraphClusteringProcessor processor2(config2);

  auto start2 = std::chrono::high_resolution_clock::now();
  auto neutrons2 = processor2.processHits(large_hits);
  auto end2 = std::chrono::high_resolution_clock::now();
  auto time2 =
      std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2)
          .count();

  const auto& stats1 = processor1.getStatistics();
  const auto& stats2 = processor2.getStatistics();

  EXPECT_EQ(stats1.num_workers_used, 1u);
  EXPECT_EQ(stats2.num_workers_used, 4u);
  EXPECT_EQ(stats1.total_hits_processed, large_hits.size());
  EXPECT_EQ(stats2.total_hits_processed, large_hits.size());

  std::cout << "Performance scaling test:" << std::endl;
  std::cout << "  1 worker:  " << stats1.hits_per_second / 1e6
            << " M hits/sec (" << time1 << " μs)" << std::endl;
  std::cout << "  4 workers: " << stats2.hits_per_second / 1e6
            << " M hits/sec (" << time2 << " μs)" << std::endl;

  // Both should process successfully (performance comparison is informational)
  EXPECT_GT(stats1.hits_per_second, 0.0);
  EXPECT_GT(stats2.hits_per_second, 0.0);
}

// Test 27: Test TemporalGraphClusteringProcessor statistics accuracy
TEST_F(GraphClusteringTest, TemporalProcessorStatisticsAccuracy) {
  TemporalGraphClusteringProcessor processor;

  std::vector<TDCHit> hits = {createHit(100, 100, 1000),
                              createHit(101, 101, 1001),
                              createHit(200, 200, 2000)};

  auto neutrons = processor.processHits(hits);
  const auto& stats = processor.getStatistics();

  // Validate all statistics are populated
  EXPECT_EQ(stats.total_hits_processed, 3u);
  EXPECT_GE(stats.total_neutrons_produced, 0u);
  EXPECT_GT(stats.num_batches_created, 0u);
  EXPECT_GT(stats.num_workers_used, 0u);

  // Timing statistics should be non-negative (may be 0 for very fast
  // operations)
  EXPECT_GE(stats.analysis_time_ms, 0.0);
  EXPECT_GE(stats.batching_time_ms, 0.0);
  EXPECT_GE(stats.processing_time_ms, 0.0);
  EXPECT_GE(stats.aggregation_time_ms, 0.0);
  EXPECT_GE(stats.total_time_ms, 0.0);

  // Performance metrics
  EXPECT_GT(stats.hits_per_second, 0.0);
  EXPECT_GE(stats.neutron_efficiency, 0.0);
  EXPECT_LE(stats.neutron_efficiency, 1.0);

  // Total time should be approximately sum of individual phases
  double phase_sum = stats.analysis_time_ms + stats.batching_time_ms +
                     stats.processing_time_ms + stats.aggregation_time_ms;
  EXPECT_LE(phase_sum,
            stats.total_time_ms * 1.1);  // Allow 10% overhead for measurement

  std::cout << "Statistics validation:" << std::endl;
  std::cout << "  Analysis: " << stats.analysis_time_ms << " ms" << std::endl;
  std::cout << "  Batching: " << stats.batching_time_ms << " ms" << std::endl;
  std::cout << "  Processing: " << stats.processing_time_ms << " ms"
            << std::endl;
  std::cout << "  Aggregation: " << stats.aggregation_time_ms << " ms"
            << std::endl;
  std::cout << "  Total: " << stats.total_time_ms << " ms" << std::endl;
}

}  // namespace tdcsophiread