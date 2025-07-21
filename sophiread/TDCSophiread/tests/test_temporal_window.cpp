// TDCSophiread TemporalWindow Tests
// TDD approach: Tests written FIRST to specify behavior

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "tdc_hit.h"
#include "tdc_temporal_window.h"

using namespace tdcsophiread;

// Test fixture for TemporalWindow tests
class TemporalWindowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Default configuration: 40μs processing window, 75ns clustering window
    processing_window_tdc = 1600;  // 40μs = 1600 * 25ns
    clustering_window_tdc = 3;     // 75ns = 3 * 25ns
    max_hits = 1000;

    temporal_window = std::make_unique<TemporalWindow>(
        processing_window_tdc, clustering_window_tdc, max_hits);
  }

  // Helper function to create test hits
  TDCHit createHit(uint32_t tdc_timestamp, uint16_t x = 100, uint16_t y = 100,
                   uint16_t tot = 50, uint8_t chip_id = 0) {
    TDCHit hit;
    hit.x = x;
    hit.y = y;
    hit.timestamp = tdc_timestamp;
    hit.tot = tot;
    hit.chip_id = chip_id;
    return hit;
  }

  // Helper to create sequence of hits with increasing timestamps
  std::vector<TDCHit> createHitSequence(uint32_t start_tdc, uint32_t count,
                                        uint32_t interval = 1) {
    std::vector<TDCHit> hits;
    for (uint32_t i = 0; i < count; ++i) {
      hits.push_back(createHit(start_tdc + i * interval));
    }
    return hits;
  }

  uint32_t processing_window_tdc;
  uint32_t clustering_window_tdc;
  size_t max_hits;
  std::unique_ptr<TemporalWindow> temporal_window;
};

// =========================== BASIC FUNCTIONALITY ===========================

TEST_F(TemporalWindowTest, InitialStateIsEmpty) {
  // SPECIFY: New temporal window should be empty
  EXPECT_TRUE(temporal_window->empty());
  EXPECT_EQ(temporal_window->getCurrentHitCount(), 0);
  EXPECT_FALSE(temporal_window->hasCompletableHits());

  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.hits_in_window, 0);
  EXPECT_EQ(stats.total_hits_processed, 0);
  EXPECT_EQ(stats.hits_released, 0);
  EXPECT_EQ(stats.hits_pending, 0);
}

TEST_F(TemporalWindowTest, CanAddSingleHit) {
  // SPECIFY: Should be able to add a single hit
  std::vector<TDCHit> hits = {createHit(1000)};

  size_t added = temporal_window->addHits(hits);

  EXPECT_EQ(added, 1);
  EXPECT_FALSE(temporal_window->empty());
  EXPECT_EQ(temporal_window->getCurrentHitCount(), 1);

  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.hits_in_window, 1);
  EXPECT_EQ(stats.total_hits_processed, 1);
}

TEST_F(TemporalWindowTest, CanAddMultipleHits) {
  // SPECIFY: Should be able to add multiple hits in sequence
  auto hits = createHitSequence(1000, 5);

  size_t added = temporal_window->addHits(hits);

  EXPECT_EQ(added, 5);
  EXPECT_EQ(temporal_window->getCurrentHitCount(), 5);

  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.hits_in_window, 5);
  EXPECT_EQ(stats.total_hits_processed, 5);
}

// =========================== WINDOW MANAGEMENT ===========================

TEST_F(TemporalWindowTest, WindowBoundariesAreSetCorrectly) {
  // SPECIFY: Window boundaries should be established when first hit is added
  auto hits = createHitSequence(1000, 3);
  temporal_window->addHits(hits);

  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.window_start_tdc, 1000);
  EXPECT_EQ(stats.window_end_tdc, 1000 + processing_window_tdc);
  EXPECT_NEAR(stats.window_duration_ms, 0.04, 0.001);  // 40μs = 0.04ms
}

TEST_F(TemporalWindowTest, WindowAdvancesWithNewHits) {
  // SPECIFY: Window should advance when hits exceed current window
  auto initial_hits = createHitSequence(1000, 2);
  temporal_window->addHits(initial_hits);

  // Add hits that force window advancement
  auto later_hits = createHitSequence(1000 + processing_window_tdc + 100, 2);
  temporal_window->addHits(later_hits);

  auto stats = temporal_window->getStats();
  EXPECT_GT(stats.window_start_tdc, 1000);  // Window should have advanced
}

TEST_F(TemporalWindowTest, HitsOutsideProcessingWindowAreDiscarded) {
  // SPECIFY: Hits too far outside processing window should be discarded
  // automatically
  auto initial_hits = createHitSequence(1000, 2);
  temporal_window->addHits(initial_hits);

  // Add hits way beyond processing window
  auto far_future_hits = createHitSequence(1000 + processing_window_tdc * 3, 2);
  temporal_window->addHits(far_future_hits);

  // Should have discarded old hits when window advanced
  EXPECT_LT(temporal_window->getCurrentHitCount(), 4);
}

// =========================== COMPLETABLE HITS LOGIC
// ===========================

TEST_F(TemporalWindowTest, HitsWithinClusteringWindowAreNotCompletable) {
  // SPECIFY: Hits within clustering window should not be completable yet
  auto hits = createHitSequence(1000, 3, 1);  // 3 hits, 1 TDC unit apart
  temporal_window->addHits(hits);

  // No hits should be completable yet (all within clustering window)
  EXPECT_FALSE(temporal_window->hasCompletableHits());
  auto completable = temporal_window->getCompletableHits();
  EXPECT_TRUE(completable.empty());
}

TEST_F(TemporalWindowTest, HitsOutsideClusteringWindowAreCompletable) {
  // SPECIFY: Hits outside clustering window should become completable
  auto early_hits = createHitSequence(1000, 2);
  temporal_window->addHits(early_hits);

  // Add hits beyond clustering window
  auto later_hits = createHitSequence(1000 + clustering_window_tdc + 5, 2);
  temporal_window->addHits(later_hits);

  EXPECT_TRUE(temporal_window->hasCompletableHits());
  auto completable = temporal_window->getCompletableHits();
  EXPECT_EQ(completable.size(), 2);  // Early hits should be completable

  // Verify the hits are the early ones
  EXPECT_EQ(completable[0].timestamp, 1000);
  EXPECT_EQ(completable[1].timestamp, 1001);
}

TEST_F(TemporalWindowTest, CompletableHitsAreRemovedFromWindow) {
  // SPECIFY: Getting completable hits should remove them from window
  auto early_hits = createHitSequence(1000, 2);
  temporal_window->addHits(early_hits);

  auto later_hits = createHitSequence(1000 + clustering_window_tdc + 5, 2);
  temporal_window->addHits(later_hits);

  size_t initial_count = temporal_window->getCurrentHitCount();
  auto completable = temporal_window->getCompletableHits();
  size_t after_count = temporal_window->getCurrentHitCount();

  EXPECT_EQ(after_count, initial_count - completable.size());

  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.hits_released, completable.size());
}

// =========================== FORCED WINDOW ADVANCEMENT
// ===========================

TEST_F(TemporalWindowTest, CanForceAdvanceWindow) {
  // SPECIFY: Should be able to force window advancement
  auto hits = createHitSequence(1000, 5);
  temporal_window->addHits(hits);

  uint32_t new_end = 1000 + processing_window_tdc * 2;
  size_t became_completable = temporal_window->advanceWindow(new_end);

  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.window_end_tdc, new_end);
  EXPECT_GT(became_completable, 0);  // Some hits should have become completable
}

// =========================== MEMORY MANAGEMENT ===========================

TEST_F(TemporalWindowTest, RespectsMaxHitsLimit) {
  // SPECIFY: Should not exceed maximum hits limit
  size_t max_hits_small = 10;
  auto small_window = TemporalWindow(processing_window_tdc,
                                     clustering_window_tdc, max_hits_small);

  auto many_hits = createHitSequence(1000, 20);  // More than limit
  size_t added = small_window.addHits(many_hits);

  EXPECT_LE(added, max_hits_small);
  EXPECT_LE(small_window.getCurrentHitCount(), max_hits_small);
}

TEST_F(TemporalWindowTest, ReportsMemoryUsageCorrectly) {
  // SPECIFY: Should accurately report memory usage
  auto hits = createHitSequence(1000, 10);
  temporal_window->addHits(hits);

  size_t expected_memory = 10 * sizeof(TDCHit);
  EXPECT_EQ(temporal_window->getMemoryUsage(), expected_memory);

  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.memory_usage_bytes, expected_memory);
}

TEST_F(TemporalWindowTest, DetectsNearCapacity) {
  // SPECIFY: Should detect when near memory capacity
  size_t max_hits_small = 10;
  auto small_window = TemporalWindow(processing_window_tdc,
                                     clustering_window_tdc, max_hits_small);

  auto hits = createHitSequence(1000, 9);  // 90% of capacity
  small_window.addHits(hits);

  EXPECT_TRUE(small_window.isNearCapacity(0.8));    // Above 80% threshold
  EXPECT_FALSE(small_window.isNearCapacity(0.95));  // Below 95% threshold
}

// =========================== FINAL PROCESSING ===========================

TEST_F(TemporalWindowTest, CanGetAllPendingHits) {
  // SPECIFY: Should be able to get all remaining hits for final processing
  auto hits = createHitSequence(1000, 5);
  temporal_window->addHits(hits);

  auto all_pending = temporal_window->getAllPendingHits();

  EXPECT_EQ(all_pending.size(), 5);
  EXPECT_TRUE(temporal_window
                  ->empty());  // Window should be empty after getting all hits

  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.hits_in_window, 0);
}

// =========================== UTILITY FUNCTIONS ===========================

TEST_F(TemporalWindowTest, TDCToMillisecondsConversion) {
  // SPECIFY: Should correctly convert TDC units to milliseconds
  uint32_t tdc_units = 1600;  // 40μs
  double ms = TemporalWindow::tdcToMilliseconds(tdc_units);
  EXPECT_NEAR(ms, 0.04, 0.001);  // 40μs = 0.04ms
}

TEST_F(TemporalWindowTest, MillisecondsToTDCConversion) {
  // SPECIFY: Should correctly convert milliseconds to TDC units
  double ms = 0.04;  // 40μs
  uint32_t tdc_units = TemporalWindow::millisecondsToTdc(ms);
  EXPECT_EQ(tdc_units, 1600);
}

// =========================== EDGE CASES ===========================

TEST_F(TemporalWindowTest, HandlesEmptyHitVector) {
  // SPECIFY: Should handle empty hit vectors gracefully
  std::vector<TDCHit> empty_hits;
  size_t added = temporal_window->addHits(empty_hits);

  EXPECT_EQ(added, 0);
  EXPECT_TRUE(temporal_window->empty());
}

TEST_F(TemporalWindowTest, HandlesUnsortedHits) {
  // SPECIFY: Should handle unsorted hits (implementation should sort them)
  std::vector<TDCHit> unsorted_hits = {createHit(1005), createHit(1001),
                                       createHit(1003), createHit(1000)};

  size_t added = temporal_window->addHits(unsorted_hits);
  EXPECT_EQ(added, 4);

  // Implementation should sort them internally
  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.window_start_tdc, 1000);  // Should start from earliest hit
}

TEST_F(TemporalWindowTest, ClearFunctionWorksCorrectly) {
  // SPECIFY: Clear should reset window to initial state
  auto hits = createHitSequence(1000, 5);
  temporal_window->addHits(hits);

  temporal_window->clear();

  EXPECT_TRUE(temporal_window->empty());
  EXPECT_EQ(temporal_window->getCurrentHitCount(), 0);

  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.hits_in_window, 0);
  EXPECT_EQ(stats.window_start_tdc, 0);
  EXPECT_EQ(stats.window_end_tdc, 0);
}

// =========================== STATISTICS VALIDATION ===========================

TEST_F(TemporalWindowTest, StatisticsAreAccurate) {
  // SPECIFY: Statistics should accurately reflect window state
  auto hits1 = createHitSequence(1000, 3);
  temporal_window->addHits(hits1);

  auto hits2 = createHitSequence(1000 + clustering_window_tdc + 10, 2);
  temporal_window->addHits(hits2);

  auto completable = temporal_window->getCompletableHits();

  auto stats = temporal_window->getStats();
  EXPECT_EQ(stats.total_hits_processed, 5);
  EXPECT_EQ(stats.hits_released, completable.size());
  EXPECT_EQ(stats.hits_pending, temporal_window->getCurrentHitCount());
  EXPECT_EQ(stats.hits_in_window, temporal_window->getCurrentHitCount());
}