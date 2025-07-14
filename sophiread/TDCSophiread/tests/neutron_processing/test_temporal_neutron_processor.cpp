// TDCSophiread Temporal Neutron Processor Tests (Stateless Architecture)
// Tests for stateless parallel neutron processing with zero-copy batching

#include <gtest/gtest.h>

#include "neutron_processing/neutron_factories.h"
#include "neutron_processing/neutron_processing.h"
#include "tdc_hit.h"

using namespace tdcsophiread;

class TemporalNeutronProcessorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test data with realistic TPX3 timing structure
    createTestHits();

    // Create temporal processor configuration for stateless processing
    config_.clustering.algorithm = "abs";
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

// Test 1: Basic processor creation and configuration
TEST_F(TemporalNeutronProcessorTest, CreatesTemporalProcessor) {
  auto processor = NeutronProcessorFactory::create(config_);

  EXPECT_NE(processor, nullptr);
  EXPECT_EQ(processor->getHitClusteringAlgorithm(), "abs");
  EXPECT_EQ(processor->getNeutronExtractionAlgorithm(), "simple_centroid");

  // Check that it's actually a temporal processor
  auto* temporal_processor =
      dynamic_cast<TemporalNeutronProcessor*>(processor.get());
  EXPECT_NE(temporal_processor, nullptr);
  EXPECT_EQ(temporal_processor->getNumWorkers(), 4);
}

// Test 2: Basic processing functionality
TEST_F(TemporalNeutronProcessorTest, ProcessesHitsSuccessfully) {
  TemporalNeutronProcessor processor(config_);

  auto neutrons = processor.processHits(test_hits_);

  // Verify processing produced results
  EXPECT_FALSE(neutrons.empty());
  EXPECT_GT(neutrons.size(), 0);

  // Basic sanity checks on neutrons
  for (const auto& neutron : neutrons) {
    EXPECT_GE(neutron.x, 0.0);
    EXPECT_GE(neutron.y, 0.0);
    EXPECT_GT(neutron.n_hits, 0);
    EXPECT_GT(neutron.tot, 0);
  }
}

// Test 3: Basic performance metrics
TEST_F(TemporalNeutronProcessorTest, ProvideBasicMetrics) {
  TemporalNeutronProcessor processor(config_);

  auto neutrons = processor.processHits(test_hits_);

  // Verify basic performance metrics are available
  EXPECT_GT(processor.getLastProcessingTimeMs(), 0.0);
  EXPECT_GT(processor.getLastHitsPerSecond(), 0.0);
  EXPECT_GE(processor.getLastNeutronEfficiency(), 0.0);
  EXPECT_LE(processor.getLastNeutronEfficiency(), 1.0);

  // Verify basic statistics
  auto stats = processor.getStatistics();
  EXPECT_EQ(stats.total_hits_processed, test_hits_.size());
  EXPECT_EQ(stats.total_neutrons_produced, neutrons.size());
  EXPECT_GT(stats.total_processing_time_ms, 0.0);
}

// Test 4: Empty input handling
TEST_F(TemporalNeutronProcessorTest, HandlesEmptyInput) {
  TemporalNeutronProcessor processor(config_);

  std::vector<TDCHit> empty_hits;
  auto neutrons = processor.processHits(empty_hits);

  EXPECT_TRUE(neutrons.empty());

  auto stats = processor.getStatistics();
  EXPECT_EQ(stats.total_hits_processed, 0);
  EXPECT_EQ(stats.total_neutrons_produced, 0);
}

// Test 5: Single hit processing
TEST_F(TemporalNeutronProcessorTest, ProcessesSingleHit) {
  TemporalNeutronProcessor processor(config_);

  std::vector<TDCHit> single_hit = {test_hits_[0]};
  auto neutrons = processor.processHits(single_hit);

  // Should produce one neutron from one hit (min_cluster_size = 1)
  EXPECT_EQ(neutrons.size(), 1);

  auto stats = processor.getStatistics();
  EXPECT_EQ(stats.total_hits_processed, 1);
  EXPECT_EQ(stats.total_neutrons_produced, 1);
}

// Test 6: Configuration management
TEST_F(TemporalNeutronProcessorTest, ManagesConfiguration) {
  TemporalNeutronProcessor processor(config_);

  const auto& proc_config = processor.getConfig();
  EXPECT_EQ(proc_config.clustering.algorithm, "abs");
  EXPECT_EQ(proc_config.extraction.algorithm, "simple_centroid");
  EXPECT_EQ(proc_config.temporal.num_workers, 4);

  // Test reconfiguration
  NeutronProcessingConfig new_config = config_;
  new_config.temporal.num_workers = 2;
  new_config.temporal.enable_deduplication = false;

  processor.configure(new_config);

  EXPECT_EQ(processor.getNumWorkers(), 2);
  EXPECT_FALSE(processor.getConfig().temporal.enable_deduplication);
}

// Test 7: Deterministic results
TEST_F(TemporalNeutronProcessorTest, ProducesDeterministicResults) {
  TemporalNeutronProcessor processor(config_);

  // Process the same data multiple times
  auto neutrons1 = processor.processHits(test_hits_);
  processor.reset();
  auto neutrons2 = processor.processHits(test_hits_);

  // Results should be identical (deterministic)
  EXPECT_EQ(neutrons1.size(), neutrons2.size());
}

// Test 8: Deduplication functionality
TEST_F(TemporalNeutronProcessorTest, DeduplicationWorks) {
  // Test with deduplication enabled
  config_.temporal.enable_deduplication = true;
  TemporalNeutronProcessor processor_with_dedup(config_);

  auto neutrons_with_dedup = processor_with_dedup.processHits(test_hits_);

  // Test with deduplication disabled
  config_.temporal.enable_deduplication = false;
  TemporalNeutronProcessor processor_no_dedup(config_);

  auto neutrons_no_dedup = processor_no_dedup.processHits(test_hits_);

  // Should produce similar results
  EXPECT_GT(neutrons_with_dedup.size(), 0);
  EXPECT_GT(neutrons_no_dedup.size(), 0);

  // Deduplication should not increase neutron count
  EXPECT_LE(neutrons_with_dedup.size(), neutrons_no_dedup.size());
}

// Test 9: Factory selection logic
TEST_F(TemporalNeutronProcessorTest, FactoryCreatesCorrectProcessor) {
  // All configurations should create TemporalNeutronProcessor (stateless only)

  // Single-threaded configuration
  NeutronProcessingConfig single_config = config_;
  single_config.temporal.num_workers = 1;

  auto single_processor = NeutronProcessorFactory::create(single_config);
  auto* temporal_single =
      dynamic_cast<TemporalNeutronProcessor*>(single_processor.get());
  EXPECT_NE(temporal_single, nullptr);
  EXPECT_EQ(temporal_single->getNumWorkers(), 1);

  // Multi-threaded configuration
  auto multi_processor = NeutronProcessorFactory::create(config_);
  auto* temporal_multi =
      dynamic_cast<TemporalNeutronProcessor*>(multi_processor.get());
  EXPECT_NE(temporal_multi, nullptr);
  EXPECT_EQ(temporal_multi->getNumWorkers(), 4);

  // Auto-detection (0 workers)
  NeutronProcessingConfig auto_config = config_;
  auto_config.temporal.num_workers = 0;

  auto auto_processor = NeutronProcessorFactory::create(auto_config);
  auto* temporal_auto =
      dynamic_cast<TemporalNeutronProcessor*>(auto_processor.get());
  EXPECT_NE(temporal_auto, nullptr);
  EXPECT_GT(temporal_auto->getNumWorkers(), 0);
}

// Test 10: Worker pool size handling
TEST_F(TemporalNeutronProcessorTest, HandlesVariableWorkerCounts) {
  // Test different worker counts
  for (size_t workers : {1, 2, 4, 8}) {
    config_.temporal.num_workers = workers;
    TemporalNeutronProcessor processor(config_);

    EXPECT_EQ(processor.getNumWorkers(), workers);

    // Should still process hits correctly
    auto neutrons = processor.processHits(test_hits_);
    EXPECT_GT(neutrons.size(), 0);
  }
}