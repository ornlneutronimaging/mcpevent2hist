// TDCSophiread Hit Batching Implementation
// Zero-copy batch creation for parallel temporal processing

#include <algorithm>
#include <cmath>

#include "neutron_processing/clustering_state.h"
#include "neutron_processing/hit_clustering.h"

namespace tdcsophiread {
namespace TemporalBatching {

std::vector<HitBatch> createStatisticalBatches(
    const std::vector<TDCHit>* hits_ptr,
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end, const BatchStatistics& stats) {
  std::vector<HitBatch> batches;

  if (!hits_ptr || begin >= end || stats.mean_hits_per_window == 0.0) {
    return batches;
  }

  size_t total_hits = std::distance(begin, end);
  size_t start_offset = std::distance(hits_ptr->begin(), begin);

  size_t estimated_hits_per_batch =
      static_cast<size_t>(stats.mean_hits_per_window * 4);
  estimated_hits_per_batch = std::max(estimated_hits_per_batch, size_t(1000));

  size_t current_start = 0;
  uint32_t current_tof_start = begin->tof;

  while (current_start < total_hits) {
    HitBatch batch;
    batch.hits_ptr = hits_ptr;
    batch.start_index = start_offset + current_start;
    batch.tof_window_start = current_tof_start;

    size_t estimated_end =
        std::min(current_start + estimated_hits_per_batch, total_hits);

    size_t actual_end = estimated_end;
    if (estimated_end < total_hits) {
      auto estimated_it = begin + estimated_end;
      auto scan_it = estimated_it;

      uint32_t correlation_window_tof = stats.optimal_window_tof;
      size_t lookback_limit = std::min(size_t(1000), estimated_end / 4);

      for (size_t lookback = 0;
           lookback < lookback_limit && scan_it > begin + current_start;
           ++lookback, --scan_it) {
        auto next_it = scan_it + 1;
        if (next_it < end &&
            next_it->tof > scan_it->tof + correlation_window_tof) {
          actual_end = std::distance(begin, next_it);
          break;
        }
      }
    }

    batch.end_index = start_offset + actual_end;
    batch.tof_window_end = (actual_end < total_hits)
                               ? (begin + actual_end - 1)->tof
                               : (end - 1)->tof;

    if (batches.empty()) {
      batch.overlap_start = batch.start_index;
    } else {
      size_t overlap_hits = std::min(
          stats.overlap_size, batch.start_index - batches.back().start_index);
      batch.overlap_start = batch.start_index - overlap_hits;
    }

    size_t remaining_hits = total_hits - actual_end;
    if (remaining_hits > 0) {
      size_t overlap_hits = std::min(stats.overlap_size, remaining_hits);
      batch.overlap_end = batch.end_index + overlap_hits;
    } else {
      batch.overlap_end = batch.end_index;
    }

    batch.overlap_end = std::min(batch.overlap_end, start_offset + total_hits);

    batches.push_back(std::move(batch));
    current_start = actual_end;
    if (current_start < total_hits) {
      current_tof_start = (begin + current_start)->tof;
    }
  }

  return batches;
}

std::vector<HitBatch> createFixedSizeBatches(
    const std::vector<TDCHit>* hits_ptr,
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end, size_t batch_size,
    size_t overlap_size) {
  std::vector<HitBatch> batches;

  // Validate inputs
  if (!hits_ptr || begin >= end || batch_size == 0) {
    return batches;
  }

  size_t total_hits = std::distance(begin, end);
  size_t start_offset = std::distance(hits_ptr->begin(), begin);

  size_t current_start = 0;  // Relative to begin iterator

  while (current_start < total_hits) {
    HitBatch batch;
    batch.hits_ptr = hits_ptr;
    batch.start_index = start_offset + current_start;

    // Calculate batch end
    size_t current_end = std::min(current_start + batch_size, total_hits);
    batch.end_index = start_offset + current_end;

    // Set TOF window
    batch.tof_window_start = (begin + current_start)->tof;
    batch.tof_window_end = (begin + current_end - 1)->tof;

    // Calculate overlap regions
    if (batches.empty()) {
      // First batch - no overlap at start
      batch.overlap_start = batch.start_index;
    } else {
      // Overlap with previous batch
      size_t actual_overlap = std::min(
          overlap_size, batch.start_index - batches.back().start_index);
      batch.overlap_start = batch.start_index - actual_overlap;
    }

    // Calculate overlap end
    size_t remaining_hits = total_hits - current_end;
    if (remaining_hits > 0) {
      size_t actual_overlap = std::min(overlap_size, remaining_hits);
      batch.overlap_end = batch.end_index + actual_overlap;
    } else {
      batch.overlap_end = batch.end_index;
    }

    // Ensure overlap end doesn't exceed bounds
    batch.overlap_end = std::min(batch.overlap_end, start_offset + total_hits);

    batches.push_back(std::move(batch));

    // Move to next batch
    current_start = current_end;
  }

  return batches;
}

}  // namespace TemporalBatching
}  // namespace tdcsophiread