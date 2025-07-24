// TDCSophiread Temporal Window Header
// TDD Step 3: Full implementation to make tests pass

#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <vector>

#include "tdc_hit.h"

namespace tdcsophiread {

/**
 * @brief Statistics for temporal window
 */
struct TemporalWindowStats {
  size_t hits_in_window = 0;
  size_t total_hits_processed = 0;
  size_t hits_released = 0;
  size_t hits_pending = 0;
  uint32_t window_start_tdc = 0;
  uint32_t window_end_tdc = 0;
  double window_duration_ms = 0.0;
  size_t memory_usage_bytes = 0;
};

/**
 * @brief Temporal window for clustering with bounded memory
 *
 * This class manages a sliding temporal window for neutron clustering
 * that prevents unbounded memory growth during streaming processing.
 *
 * Key Features:
 * - Sliding temporal window based on TDC timestamps
 * - Bounded memory usage (prevents accumulation)
 * - Efficient hit release for clustering
 * - Configurable window size
 * - Statistics tracking
 *
 * Algorithm:
 * 1. Hits are added to window based on TDC timestamp
 * 2. Window advances when new hits exceed window duration
 * 3. Complete hits (outside clustering window) are released
 * 4. Pending hits (might cluster with future hits) are retained
 *
 * Window Design:
 * - Processing Window: Large window (e.g., 40μs) for temporal grouping
 * - Clustering Window: Small window (e.g., 75ns) for actual clustering
 * - Hits are released when they're outside the clustering window
 *
 * Memory Bound:
 * - Only keeps hits within the processing window
 * - Automatically releases old hits
 * - Configurable maximum window size
 */
class TemporalWindow {
 public:
  TemporalWindow(uint32_t processing_window_tdc = 1600,
                 uint32_t clustering_window_tdc = 3,
                 size_t max_hits_in_window = 1000000);

  // Core functionality
  size_t addHits(const std::vector<TDCHit>& new_hits);
  std::vector<TDCHit> getCompletableHits();
  std::vector<TDCHit> getAllPendingHits();
  bool hasCompletableHits() const;
  size_t advanceWindow(uint32_t new_end_tdc);

  // State queries
  bool empty() const;
  size_t getCurrentHitCount() const;
  size_t getMemoryUsage() const;
  bool isNearCapacity(double threshold_fraction = 0.8) const;
  TemporalWindowStats getStats() const;
  void clear();

  // Static utilities
  static double tdcToMilliseconds(uint32_t tdc_units);
  static uint32_t millisecondsToTdc(double milliseconds);

 private:
  // Helper methods
  void updateWindowBoundaries(uint32_t latest_tdc);
  size_t removeOldHits();
  std::deque<TDCHit>::iterator findCompletableHitsEnd();

  // Configuration
  uint32_t m_ProcessingWindowTdc;
  uint32_t m_ClusteringWindowTdc;
  size_t m_MaxHitsInWindow;

  // Window state
  std::deque<TDCHit>
      m_HitsBuffer;  // Hits in temporal window (sorted by timestamp)
  uint32_t m_WindowStartTdc = 0;
  uint32_t m_WindowEndTdc = 0;

  // Statistics
  size_t m_TotalHitsProcessed = 0;
  size_t m_HitsReleased = 0;
};

}  // namespace tdcsophiread