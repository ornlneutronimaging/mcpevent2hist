// TDCSophiread Hit Batching Implementation
// Zero-copy batch creation for parallel temporal processing

#include <algorithm>
#include <cmath>

#include "neutron_processing/clustering_state.h"
#include "neutron_processing/hit_clustering.h"

namespace tdcsophiread {
namespace TemporalBatching {

// createStatisticalBatches removed - using simple fixed batching instead

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