// TDCSophiread Temporal Neutron Processor Unit Tests
// Test parallel processing with worker pool architecture

#include <gtest/gtest.h>

#include "neutron_processing/basic_neutron_processor.h"
#include "neutron_processing/neutron_factories.h"
#include "neutron_processing/neutron_processing.h"
#include "tdc_hit.h"

using namespace tdcsophiread;

class TemporalNeutronProcessorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test data with realistic TPX3 timing structure
    createTestHits();

    // Create temporal processor configuration
    config_.clustering.algorithm = "simple_abs";
    config_.clustering.abs.radius = 5.0;
    config_.clustering.abs.min_cluster_size = 1;
    config_.clustering.abs.neutron_correlation_window = 75.0;

    config_.extraction.algorithm = "simple_centroid";
    config_.extraction.super_resolution_factor = 8.0;
    config_.extraction.weighted_by_tot = true;

    config_.temporal.num_workers = 4;  // Force parallel processing
    config_.temporal.min_batch_size = 10;
    config_.temporal.max_batch_size = 50;
    config_.temporal.overlap_factor = 3.0;
    config_.temporal.enable_deduplication = true;
    config_.temporal.deduplication_tolerance = 1.0;
  }

  void createTestHits() {
    test_hits_.clear();

    // Create hits spanning 2 pulses with realistic distribution
    // Pulse 1: Start at TOF = 1000, build up to high TOF values
    uint32_t pulse1_start = 1000;
    uint32_t pulse1_timestamp_base = 1000;
    for (int cluster = 0; cluster < 8; ++cluster) {
      uint32_t cluster_tof =
          pulse1_start + cluster * 5000;  // TOF grows within pulse

      // Create cluster with 3-5 hits within correlation window
      int hits_in_cluster = 3 + (cluster % 3);
      for (int hit = 0; hit < hits_in_cluster; ++hit) {
        TDCHit h;
        h.x = 100 + cluster * 15 + hit;
        h.y = 200 + cluster * 10 + hit;
        h.tof = cluster_tof + hit;  // TOF increases within pulse
        h.tot = 100 + hit * 10;
        h.chip_id = 0;
        h.timestamp = pulse1_timestamp_base + cluster * 5000 + hit;
        test_hits_.push_back(h);
      }
    }

    // Pulse 2: TOF resets dramatically (TOF wraps from high to low)
    uint32_t pulse2_start = 500;  // Much lower than last pulse1 TOF
    uint32_t pulse2_timestamp_base = pulse1_timestamp_base + 100000;
    for (int cluster = 0; cluster < 6; ++cluster) {
      uint32_t cluster_tof =
          pulse2_start + cluster * 4000;  // TOF starts low again

      int hits_in_cluster = 2 + (cluster % 4);
      for (int hit = 0; hit < hits_in_cluster; ++hit) {
        TDCHit h;
        h.x = 300 + cluster * 20 + hit;
        h.y = 400 + cluster * 15 + hit;
        h.tof = cluster_tof + hit;  // TOF reset to low values
        h.tot = 120 + hit * 15;
        h.chip_id = 0;
        h.timestamp = pulse2_timestamp_base + cluster * 4000 + hit;
        test_hits_.push_back(h);
      }
    }

    // Sort by timestamp to maintain temporal order
    std::sort(test_hits_.begin(), test_hits_.end(),
              [](const TDCHit& a, const TDCHit& b) {
                return a.timestamp < b.timestamp;
              });
  }

  std::vector<TDCHit> test_hits_;
  NeutronProcessingConfig config_;
};

// Test basic temporal processor creation
TEST_F(TemporalNeutronProcessorTest, CreatesTemporalProcessor) {
  auto processor = NeutronProcessorFactory::create(config_);

  EXPECT_NE(processor, nullptr);
  EXPECT_EQ(processor->getHitClusteringAlgorithm(), "simple_abs");
  EXPECT_EQ(processor->getNeutronExtractionAlgorithm(), "simple_centroid");

  // Check that it's actually a temporal processor (has multiple workers)
  auto* temporal_processor =
      dynamic_cast<TemporalNeutronProcessor*>(processor.get());
  EXPECT_NE(temporal_processor, nullptr);
  EXPECT_EQ(temporal_processor->getNumWorkers(), 4);
}

// Test configuration propagation
TEST_F(TemporalNeutronProcessorTest, ConfigurationPropagation) {
  TemporalNeutronProcessor processor(config_);

  const auto& proc_config = processor.getConfig();
  EXPECT_EQ(proc_config.clustering.algorithm, "simple_abs");
  EXPECT_EQ(proc_config.extraction.algorithm, "simple_centroid");
  EXPECT_EQ(proc_config.temporal.num_workers, 4);
  EXPECT_EQ(proc_config.temporal.min_batch_size, 10);
  EXPECT_TRUE(proc_config.temporal.enable_deduplication);

  // Test reconfiguration
  NeutronProcessingConfig new_config = config_;
  new_config.temporal.num_workers = 2;
  new_config.temporal.enable_deduplication = false;

  processor.configure(new_config);

  EXPECT_EQ(processor.getNumWorkers(), 2);
  EXPECT_FALSE(processor.getConfig().temporal.enable_deduplication);
}

// Test parallel processing functionality
TEST_F(TemporalNeutronProcessorTest, ParallelProcessing) {
  TemporalNeutronProcessor processor(config_);

  auto neutrons = processor.processHits(test_hits_.begin(), test_hits_.end());

  // Verify processing produced results
  EXPECT_FALSE(neutrons.empty());
  EXPECT_GT(neutrons.size(), 0);

  // Verify performance statistics
  auto stats = processor.getStatistics();
  EXPECT_EQ(stats.total_hits_processed, test_hits_.size());
  EXPECT_EQ(stats.total_neutrons_produced, neutrons.size());
  EXPECT_GT(stats.total_processing_time_ms, 0.0);
  EXPECT_EQ(stats.num_workers_used, 4);
  EXPECT_GT(stats.num_batches_created, 0);

  // Verify parallel efficiency metrics
  EXPECT_GE(stats.parallel_efficiency, 0.0);
  EXPECT_LE(stats.parallel_efficiency, 1.0);
  EXPECT_GT(stats.load_balance_factor, 0.0);

  // Verify timing breakdown
  EXPECT_GT(stats.analysis_time_ms, 0.0);
  EXPECT_GT(stats.clustering_time_ms, 0.0);
  EXPECT_GT(stats.extraction_time_ms, 0.0);
  EXPECT_GT(stats.aggregation_time_ms, 0.0);
}

// Test processing with cluster labels
TEST_F(TemporalNeutronProcessorTest, ProcessingWithLabels) {
  TemporalNeutronProcessor processor(config_);

  auto results =
      processor.processHitsWithLabels(test_hits_.begin(), test_hits_.end());

  // Verify both neutrons and labels are produced
  EXPECT_FALSE(results.neutrons.empty());
  EXPECT_FALSE(results.cluster_labels.empty());
  EXPECT_EQ(results.cluster_labels.size(), test_hits_.size());

  // Verify cluster labels are properly assigned
  size_t clustered_hits = 0;
  for (int label : results.cluster_labels) {
    if (label >= 0) {
      clustered_hits++;
    }
  }
  EXPECT_GT(clustered_hits, 0);

  // Verify neutron count is reasonable relative to hits
  EXPECT_LE(results.neutrons.size(), clustered_hits);
}

// Test deduplication functionality
TEST_F(TemporalNeutronProcessorTest, Deduplication) {
  // Test with deduplication enabled
  config_.temporal.enable_deduplication = true;
  config_.temporal.deduplication_tolerance =
      2.0;  // Larger tolerance for testing
  TemporalNeutronProcessor processor_with_dedup(config_);

  auto neutrons_with_dedup =
      processor_with_dedup.processHits(test_hits_.begin(), test_hits_.end());

  // Test with deduplication disabled
  config_.temporal.enable_deduplication = false;
  TemporalNeutronProcessor processor_no_dedup(config_);

  auto neutrons_no_dedup =
      processor_no_dedup.processHits(test_hits_.begin(), test_hits_.end());

  // Should produce similar results (may be equal for this test data)
  EXPECT_GT(neutrons_with_dedup.size(), 0);
  EXPECT_GT(neutrons_no_dedup.size(), 0);

  // Deduplication should not increase neutron count
  EXPECT_LE(neutrons_with_dedup.size(), neutrons_no_dedup.size());
}

// Test worker isolation
TEST_F(TemporalNeutronProcessorTest, WorkerIsolation) {
  config_.temporal.num_workers = 2;
  TemporalNeutronProcessor processor(config_);

  // Process the same data multiple times to verify consistency
  auto neutrons1 = processor.processHits(test_hits_.begin(), test_hits_.end());
  processor.reset();
  auto neutrons2 = processor.processHits(test_hits_.begin(), test_hits_.end());

  // Results should be identical (deterministic)
  EXPECT_EQ(neutrons1.size(), neutrons2.size());

  // Verify basic consistency (positions should be similar)
  if (!neutrons1.empty() && !neutrons2.empty()) {
    // At least some neutrons should have similar properties
    bool found_similar = false;
    for (const auto& n1 : neutrons1) {
      for (const auto& n2 : neutrons2) {
        double dx = n1.x - n2.x;
        double dy = n1.y - n2.y;
        if (dx * dx + dy * dy < 100.0) {  // Within reasonable distance
          found_similar = true;
          break;
        }
      }
      if (found_similar) break;
    }
    EXPECT_TRUE(found_similar);
  }
}

// Test empty input handling
TEST_F(TemporalNeutronProcessorTest, EmptyInputHandling) {
  TemporalNeutronProcessor processor(config_);

  std::vector<TDCHit> empty_hits;
  auto neutrons = processor.processHits(empty_hits.begin(), empty_hits.end());

  EXPECT_TRUE(neutrons.empty());

  auto stats = processor.getStatistics();
  EXPECT_EQ(stats.total_hits_processed, 0);
  EXPECT_EQ(stats.total_neutrons_produced, 0);
  EXPECT_EQ(stats.num_batches_created,
            1);  // Always creates at least one batch conceptually
}

// Test single hit processing
TEST_F(TemporalNeutronProcessorTest, SingleHitProcessing) {
  TemporalNeutronProcessor processor(config_);

  std::vector<TDCHit> single_hit = {test_hits_[0]};
  auto neutrons = processor.processHits(single_hit.begin(), single_hit.end());

  // Should produce one neutron from one hit
  EXPECT_EQ(neutrons.size(), 1);

  auto stats = processor.getStatistics();
  EXPECT_EQ(stats.total_hits_processed, 1);
  EXPECT_EQ(stats.total_neutrons_produced, 1);
}

// Test performance metrics calculation
TEST_F(TemporalNeutronProcessorTest, PerformanceMetrics) {
  TemporalNeutronProcessor processor(config_);

  auto neutrons = processor.processHits(test_hits_.begin(), test_hits_.end());

  // Verify all performance metrics are reasonable
  EXPECT_GT(processor.getLastProcessingTimeMs(), 0.0);
  EXPECT_GT(processor.getLastHitsPerSecond(), 0.0);
  EXPECT_GE(processor.getLastNeutronEfficiency(), 0.0);
  EXPECT_LE(processor.getLastNeutronEfficiency(), 1.0);

  auto stats = processor.getStatistics();

  // Verify memory metrics
  EXPECT_GT(stats.peak_memory_usage_mb, 0.0);
  EXPECT_GT(stats.memory_per_worker_mb, 0.0);
  EXPECT_LE(stats.memory_per_worker_mb, stats.peak_memory_usage_mb);

  // Verify timing breakdown adds up reasonably
  double total_phase_time = stats.analysis_time_ms + stats.batching_time_ms +
                            stats.clustering_time_ms +
                            stats.extraction_time_ms +
                            stats.aggregation_time_ms;
  EXPECT_LE(total_phase_time,
            stats.total_processing_time_ms * 1.1);  // Allow some variance
}

// Test reset functionality
TEST_F(TemporalNeutronProcessorTest, ResetFunctionality) {
  TemporalNeutronProcessor processor(config_);

  // Process some data
  auto neutrons = processor.processHits(test_hits_.begin(), test_hits_.end());
  EXPECT_FALSE(neutrons.empty());

  auto stats_before = processor.getStatistics();
  EXPECT_GT(stats_before.total_hits_processed, 0);

  // Reset and verify state is cleared
  processor.reset();

  // Statistics should be reset but structure preserved
  EXPECT_EQ(processor.getNumWorkers(), 4);  // Configuration preserved
  EXPECT_EQ(processor.getHitClusteringAlgorithm(), "simple_abs");

  // Should be able to process again after reset
  auto neutrons_after_reset =
      processor.processHits(test_hits_.begin(), test_hits_.end());
  EXPECT_FALSE(neutrons_after_reset.empty());
  EXPECT_EQ(neutrons_after_reset.size(),
            neutrons.size());  // Should get same results
}

// Test factory selection logic
TEST_F(TemporalNeutronProcessorTest, FactorySelection) {
  // Test single-threaded configuration creates BasicNeutronProcessor
  NeutronProcessingConfig single_config = config_;
  single_config.temporal.num_workers = 1;

  auto single_processor = NeutronProcessorFactory::create(single_config);
  auto* basic_processor =
      dynamic_cast<BasicNeutronProcessor*>(single_processor.get());
  EXPECT_NE(basic_processor, nullptr);

  // Test multi-threaded configuration creates TemporalNeutronProcessor
  auto multi_processor = NeutronProcessorFactory::create(config_);
  auto* temporal_processor =
      dynamic_cast<TemporalNeutronProcessor*>(multi_processor.get());
  EXPECT_NE(temporal_processor, nullptr);

  // Test auto-detection (0 workers)
  NeutronProcessingConfig auto_config = config_;
  auto_config.temporal.num_workers = 0;

  auto auto_processor = NeutronProcessorFactory::create(auto_config);
  auto* auto_temporal_processor =
      dynamic_cast<TemporalNeutronProcessor*>(auto_processor.get());
  EXPECT_NE(auto_temporal_processor, nullptr);
  EXPECT_GT(auto_temporal_processor->getNumWorkers(), 0);
}