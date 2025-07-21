// TDCSophiread Temporal Window Implementation
// TDD Step 3: Real implementation to make tests pass

#include "tdc_temporal_window.h"

#include <algorithm>

namespace tdcsophiread {

TemporalWindow::TemporalWindow(uint32_t processing_window_tdc,
                               uint32_t clustering_window_tdc,
                               size_t max_hits_in_window)
    : m_ProcessingWindowTdc(processing_window_tdc),
      m_ClusteringWindowTdc(clustering_window_tdc),
      m_MaxHitsInWindow(max_hits_in_window) {}

size_t TemporalWindow::addHits(const std::vector<TDCHit>& new_hits) {
  if (new_hits.empty()) {
    return 0;
  }

  size_t added_count = 0;

  // Sort hits by timestamp (handles unsorted input)
  std::vector<TDCHit> sorted_hits = new_hits;
  std::sort(sorted_hits.begin(), sorted_hits.end(),
            [](const TDCHit& a, const TDCHit& b) {
              return a.timestamp < b.timestamp;
            });

  for (const auto& hit : sorted_hits) {
    // Check capacity limit
    if (m_HitsBuffer.size() >= m_MaxHitsInWindow) {
      break;
    }

    // Update window boundaries if this is first hit or hit extends window
    updateWindowBoundaries(hit.timestamp);

    // Only add hits within the processing window
    if (hit.timestamp >= m_WindowStartTdc && hit.timestamp <= m_WindowEndTdc) {
      // Insert hit in sorted order
      auto insert_pos =
          std::upper_bound(m_HitsBuffer.begin(), m_HitsBuffer.end(), hit,
                           [](const TDCHit& a, const TDCHit& b) {
                             return a.timestamp < b.timestamp;
                           });
      m_HitsBuffer.insert(insert_pos, hit);
      added_count++;
    }

    m_TotalHitsProcessed++;
  }

  // Remove old hits that are outside the processing window
  removeOldHits();

  return added_count;
}

std::vector<TDCHit> TemporalWindow::getCompletableHits() {
  std::vector<TDCHit> completable_hits;

  if (m_HitsBuffer.empty()) {
    return completable_hits;
  }

  // Find the cutoff point - hits that are outside the clustering window
  auto cutoff_it = findCompletableHitsEnd();

  // Extract completable hits
  completable_hits.assign(m_HitsBuffer.begin(), cutoff_it);

  // Remove them from the buffer
  m_HitsBuffer.erase(m_HitsBuffer.begin(), cutoff_it);

  // Update statistics
  m_HitsReleased += completable_hits.size();

  return completable_hits;
}

std::vector<TDCHit> TemporalWindow::getAllPendingHits() {
  std::vector<TDCHit> all_hits(m_HitsBuffer.begin(), m_HitsBuffer.end());
  clear();
  return all_hits;
}

bool TemporalWindow::hasCompletableHits() const {
  if (m_HitsBuffer.empty()) {
    return false;
  }

  // Check if there are hits outside the clustering window
  auto cutoff_it = const_cast<TemporalWindow*>(this)->findCompletableHitsEnd();
  return cutoff_it != m_HitsBuffer.begin();
}

size_t TemporalWindow::advanceWindow(uint32_t new_end_tdc) {
  size_t old_completable_count = 0;
  if (!m_HitsBuffer.empty()) {
    auto old_cutoff = findCompletableHitsEnd();
    old_completable_count = std::distance(m_HitsBuffer.begin(), old_cutoff);
  }

  // Force window advancement
  m_WindowEndTdc = new_end_tdc;
  m_WindowStartTdc = (new_end_tdc > m_ProcessingWindowTdc)
                         ? (new_end_tdc - m_ProcessingWindowTdc)
                         : 0;

  // Remove hits outside new window
  removeOldHits();

  // Calculate how many hits became completable
  size_t new_completable_count = 0;
  if (!m_HitsBuffer.empty()) {
    auto new_cutoff = findCompletableHitsEnd();
    new_completable_count = std::distance(m_HitsBuffer.begin(), new_cutoff);
  }

  return new_completable_count - old_completable_count;
}

bool TemporalWindow::empty() const { return m_HitsBuffer.empty(); }

size_t TemporalWindow::getCurrentHitCount() const {
  return m_HitsBuffer.size();
}

size_t TemporalWindow::getMemoryUsage() const {
  return m_HitsBuffer.size() * sizeof(TDCHit);
}

bool TemporalWindow::isNearCapacity(double threshold_fraction) const {
  return getCurrentHitCount() > (m_MaxHitsInWindow * threshold_fraction);
}

TemporalWindowStats TemporalWindow::getStats() const {
  TemporalWindowStats stats;
  stats.hits_in_window = m_HitsBuffer.size();
  stats.total_hits_processed = m_TotalHitsProcessed;
  stats.hits_released = m_HitsReleased;
  stats.hits_pending = m_HitsBuffer.size();
  stats.window_start_tdc = m_WindowStartTdc;
  stats.window_end_tdc = m_WindowEndTdc;
  stats.window_duration_ms = tdcToMilliseconds(m_ProcessingWindowTdc);
  stats.memory_usage_bytes = getMemoryUsage();
  return stats;
}

void TemporalWindow::clear() {
  m_HitsBuffer.clear();
  m_WindowStartTdc = 0;
  m_WindowEndTdc = 0;
  m_TotalHitsProcessed = 0;
  m_HitsReleased = 0;
}

double TemporalWindow::tdcToMilliseconds(uint32_t tdc_units) {
  return (tdc_units * 25.0) / 1e6;  // 25ns per TDC unit -> ms
}

uint32_t TemporalWindow::millisecondsToTdc(double milliseconds) {
  return static_cast<uint32_t>((milliseconds * 1e6) /
                               25.0);  // ms -> 25ns TDC units
}

// Private helper methods

void TemporalWindow::updateWindowBoundaries(uint32_t latest_tdc) {
  if (m_HitsBuffer.empty()) {
    // First hit - establish window
    m_WindowStartTdc = latest_tdc;
    m_WindowEndTdc = latest_tdc + m_ProcessingWindowTdc;
  } else if (latest_tdc > m_WindowEndTdc) {
    // Hit extends beyond current window - advance window
    m_WindowEndTdc = latest_tdc + m_ProcessingWindowTdc;
    m_WindowStartTdc = latest_tdc;
  }
}

size_t TemporalWindow::removeOldHits() {
  if (m_HitsBuffer.empty()) {
    return 0;
  }

  // Remove hits that are before the window start
  auto remove_end = std::lower_bound(m_HitsBuffer.begin(), m_HitsBuffer.end(),
                                     m_WindowStartTdc,
                                     [](const TDCHit& hit, uint32_t timestamp) {
                                       return hit.timestamp < timestamp;
                                     });

  size_t removed_count = std::distance(m_HitsBuffer.begin(), remove_end);
  m_HitsBuffer.erase(m_HitsBuffer.begin(), remove_end);

  return removed_count;
}

std::deque<TDCHit>::iterator TemporalWindow::findCompletableHitsEnd() {
  if (m_HitsBuffer.empty()) {
    return m_HitsBuffer.end();
  }

  // Find the latest timestamp in the buffer
  uint32_t latest_timestamp = m_HitsBuffer.back().timestamp;

  // Calculate clustering cutoff: hits before (latest - clustering_window) are
  // completable
  uint32_t clustering_cutoff = (latest_timestamp > m_ClusteringWindowTdc)
                                   ? (latest_timestamp - m_ClusteringWindowTdc)
                                   : 0;

  // Find first hit that should remain in window (after clustering cutoff)
  return std::upper_bound(m_HitsBuffer.begin(), m_HitsBuffer.end(),
                          clustering_cutoff,
                          [](uint32_t timestamp, const TDCHit& hit) {
                            return timestamp < hit.timestamp;
                          });
}

}  // namespace tdcsophiread