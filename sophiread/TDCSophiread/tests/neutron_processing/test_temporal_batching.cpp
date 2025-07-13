// TDCSophiread Temporal Batching Unit Tests
// Test temporal analysis and batch creation for parallel processing

#include <gtest/gtest.h>

#include "neutron_processing/hit_clustering.h"
#include "tdc_hit.h"

using namespace tdcsophiread;
using namespace tdcsophiread::TemporalBatching;

class TemporalBatchingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test data with realistic TPX3 timing structure
    createTestHits();
  }

  void createTestHits() {
    test_hits_.clear();

    // Create hits spanning 2 pulses (16.67ms each) with realistic distribution
    // Note: These constants are calculated but not directly used in test data
    // creation

    // Pulse 1: Start at TOF = 1000, build up to high TOF values
    uint32_t pulse1_start = 1000;
    uint32_t pulse1_timestamp_base = 1000;
    for (int cluster = 0; cluster < 5; ++cluster) {
      uint32_t cluster_tof =
          pulse1_start + cluster * 10000;  // TOF grows within pulse

      // Create cluster with 3-5 hits within correlation window
      int hits_in_cluster = 3 + (cluster % 3);
      for (int hit = 0; hit < hits_in_cluster; ++hit) {
        TDCHit h;
        h.x = 100 + cluster * 20 + hit;
        h.y = 200 + cluster * 15 + hit;
        h.tof = cluster_tof + hit;  // TOF increases within pulse
        h.tot = 100 + hit * 10;
        h.chip_id = 0;
        h.timestamp = pulse1_timestamp_base + cluster * 10000 + hit;
        test_hits_.push_back(h);
      }
    }

    // Pulse 2: TOF resets dramatically (TOF wraps from high to low)
    uint32_t pulse2_start =
        500;  // Much lower than last pulse1 TOF (around 41000)
    uint32_t pulse2_timestamp_base =
        pulse1_timestamp_base + 60000;  // Timestamp continues growing
    for (int cluster = 0; cluster < 4; ++cluster) {
      uint32_t cluster_tof =
          pulse2_start + cluster * 8000;  // TOF starts low again

      int hits_in_cluster = 2 + (cluster % 4);
      for (int hit = 0; hit < hits_in_cluster; ++hit) {
        TDCHit h;
        h.x = 300 + cluster * 25 + hit;
        h.y = 400 + cluster * 20 + hit;
        h.tof = cluster_tof + hit;  // TOF reset to low values
        h.tot = 120 + hit * 15;
        h.chip_id = 0;
        h.timestamp = pulse2_timestamp_base + cluster * 8000 + hit;
        test_hits_.push_back(h);
      }
    }

    // Sort by timestamp to maintain temporal order (critical - no TOF sorting!)
    std::sort(test_hits_.begin(), test_hits_.end(),
              [](const TDCHit& a, const TDCHit& b) {
                return a.timestamp < b.timestamp;
              });
  }

  std::vector<TDCHit> test_hits_;
};

// Test basic hit distribution analysis
TEST_F(TemporalBatchingTest, BasicAnalyzeHitDistribution) {
  auto stats =
      analyzeHitDistribution(test_hits_.begin(), test_hits_.end(), 2, 75.0);

  // Verify basic statistics are reasonable
  EXPECT_GT(stats.mean_hits_per_window, 0.0);
  EXPECT_GE(stats.std_hits_per_window, 0.0);
  EXPECT_EQ(stats.num_pulses_analyzed, 2);
  EXPECT_GT(stats.total_hits_analyzed, 0);
  EXPECT_EQ(stats.total_hits_analyzed, test_hits_.size());

  // Verify TOF calculations
  EXPECT_EQ(stats.optimal_window_tof, 3);     // 75ns / 25ns = 3
  EXPECT_GT(stats.pulse_period_tof, 600000);  // 16.67ms in 25ns units

  // Verify overlap size is reasonable (3σ)
  EXPECT_GE(stats.overlap_size, 0);
  EXPECT_LE(stats.overlap_size, 100);  // Sanity check
}

// Test empty input handling
TEST_F(TemporalBatchingTest, EmptyInputAnalysis) {
  std::vector<TDCHit> empty_hits;
  auto stats = analyzeHitDistribution(empty_hits.begin(), empty_hits.end());

  EXPECT_EQ(stats.mean_hits_per_window, 0.0);
  EXPECT_EQ(stats.std_hits_per_window, 0.0);
  EXPECT_EQ(stats.total_hits_analyzed, 0);
  EXPECT_EQ(stats.num_pulses_analyzed, 0);
}

// Test single hit analysis
TEST_F(TemporalBatchingTest, SingleHitAnalysis) {
  std::vector<TDCHit> single_hit = {test_hits_[0]};
  auto stats =
      analyzeHitDistribution(single_hit.begin(), single_hit.end(), 1, 75.0);

  EXPECT_GT(stats.mean_hits_per_window, 0.0);
  EXPECT_EQ(stats.total_hits_analyzed, 1);
  EXPECT_EQ(stats.num_pulses_analyzed, 1);
}

// Test statistical batch creation
TEST_F(TemporalBatchingTest, StatisticalBatchCreation) {
  // First analyze the hit distribution
  auto stats =
      analyzeHitDistribution(test_hits_.begin(), test_hits_.end(), 2, 75.0);

  // Create batches using statistical information
  auto batches = createStatisticalBatches(&test_hits_, test_hits_.begin(),
                                          test_hits_.end(), stats);

  // Verify batch creation succeeded
  EXPECT_FALSE(batches.empty());

  // Verify all hits are covered
  size_t total_covered_hits = 0;
  for (const auto& batch : batches) {
    EXPECT_TRUE(batch.isValid());
    EXPECT_EQ(batch.hits_ptr, &test_hits_);
    EXPECT_LT(batch.start_index, batch.end_index);
    EXPECT_LE(batch.end_index, test_hits_.size());

    total_covered_hits += batch.size();
  }

  // Verify complete coverage (may have overlaps, so >= total hits)
  EXPECT_GE(total_covered_hits, test_hits_.size());

  // Verify overlap regions are set correctly
  for (size_t i = 0; i < batches.size(); ++i) {
    const auto& batch = batches[i];

    if (i == 0) {
      // First batch should start with its own start
      EXPECT_EQ(batch.overlap_start, batch.start_index);
    } else {
      // Later batches should have overlap with previous
      EXPECT_LE(batch.overlap_start, batch.start_index);
    }

    EXPECT_GE(batch.overlap_end, batch.end_index);
    EXPECT_LE(batch.overlap_end, test_hits_.size());
  }
}

// Test fixed size batch creation
TEST_F(TemporalBatchingTest, FixedSizeBatchCreation) {
  size_t batch_size = 10;
  size_t overlap_size = 3;

  auto batches =
      createFixedSizeBatches(&test_hits_, test_hits_.begin(), test_hits_.end(),
                             batch_size, overlap_size);

  // Verify batches were created
  EXPECT_FALSE(batches.empty());

  // Calculate expected number of batches
  size_t expected_batches = (test_hits_.size() + batch_size - 1) / batch_size;
  EXPECT_EQ(batches.size(), expected_batches);

  // Verify batch sizes and coverage
  for (size_t i = 0; i < batches.size(); ++i) {
    const auto& batch = batches[i];

    EXPECT_TRUE(batch.isValid());
    EXPECT_EQ(batch.hits_ptr, &test_hits_);

    // Verify batch size (last batch may be smaller)
    if (i < batches.size() - 1) {
      EXPECT_EQ(batch.size(), batch_size);
    } else {
      EXPECT_LE(batch.size(), batch_size);
      EXPECT_GT(batch.size(), 0);
    }

    // Verify overlap calculations
    if (i > 0) {
      // Should have overlap with previous batch
      size_t expected_overlap = std::min(
          overlap_size, batch.start_index - batches[i - 1].start_index);
      EXPECT_EQ(batch.start_index - batch.overlap_start, expected_overlap);
    }
  }
}

// Test edge case: batch size larger than data
TEST_F(TemporalBatchingTest, LargeBatchSize) {
  size_t large_batch_size = test_hits_.size() * 2;

  auto batches = createFixedSizeBatches(&test_hits_, test_hits_.begin(),
                                        test_hits_.end(), large_batch_size, 0);

  // Should create exactly one batch containing all hits
  EXPECT_EQ(batches.size(), 1);
  EXPECT_EQ(batches[0].size(), test_hits_.size());
  EXPECT_EQ(batches[0].start_index, 0);
  EXPECT_EQ(batches[0].end_index, test_hits_.size());
}

// Test iterator range processing (subset of data)
TEST_F(TemporalBatchingTest, IteratorRangeProcessing) {
  // Process only middle portion of the data
  size_t start_idx = test_hits_.size() / 4;
  size_t end_idx = 3 * test_hits_.size() / 4;

  auto begin_it = test_hits_.begin() + start_idx;
  auto end_it = test_hits_.begin() + end_idx;

  // Analyze subset
  auto stats = analyzeHitDistribution(begin_it, end_it, 1, 75.0);
  EXPECT_EQ(stats.total_hits_analyzed, end_idx - start_idx);

  // Create batches for subset
  auto batches = createStatisticalBatches(&test_hits_, begin_it, end_it, stats);

  // Verify batches reference correct indices in original vector
  for (const auto& batch : batches) {
    EXPECT_GE(batch.start_index, start_idx);
    EXPECT_LE(batch.end_index, end_idx);
    EXPECT_EQ(batch.hits_ptr, &test_hits_);
  }
}

// Test invalid input handling
TEST_F(TemporalBatchingTest, InvalidInputs) {
  // Null pointer
  auto batches1 = createStatisticalBatches(nullptr, test_hits_.begin(),
                                           test_hits_.end(), BatchStatistics());
  EXPECT_TRUE(batches1.empty());

  // Invalid iterator range
  auto batches2 = createStatisticalBatches(
      &test_hits_, test_hits_.end(), test_hits_.begin(), BatchStatistics());
  EXPECT_TRUE(batches2.empty());

  // Zero batch size
  auto batches3 = createFixedSizeBatches(&test_hits_, test_hits_.begin(),
                                         test_hits_.end(), 0, 0);
  EXPECT_TRUE(batches3.empty());
}

// Test HitBatch utility methods
TEST_F(TemporalBatchingTest, HitBatchUtilities) {
  HitBatch batch;
  batch.hits_ptr = &test_hits_;
  batch.start_index = 5;
  batch.end_index = 15;

  // Test size calculation
  EXPECT_EQ(batch.size(), 10);

  // Test validity check
  EXPECT_TRUE(batch.isValid());

  // Test iterators
  auto begin_it = batch.begin();
  auto end_it = batch.end();
  EXPECT_EQ(std::distance(begin_it, end_it), 10);
  EXPECT_EQ(begin_it, test_hits_.begin() + 5);
  EXPECT_EQ(end_it, test_hits_.begin() + 15);

  // Test invalid batch
  HitBatch invalid_batch;  // Default constructor creates invalid batch
  EXPECT_FALSE(invalid_batch.isValid());
  EXPECT_EQ(invalid_batch.size(), 0);
}