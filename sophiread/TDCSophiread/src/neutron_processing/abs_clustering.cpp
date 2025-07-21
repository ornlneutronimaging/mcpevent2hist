// TDCSophiread ABS Clustering Implementation
// Primary production implementation of Age-Based Spatial clustering

#include "neutron_processing/abs_clustering.h"

#include <algorithm>
#include <stdexcept>

namespace tdcsophiread {

ABSClustering::ABSClustering() {
  // Initialize with default Venus ABS configuration
  config_ = HitClusteringConfig::venusDefaults();
  config_.algorithm = "abs";
  config_.validate();
}

ABSClustering::ABSClustering(const HitClusteringConfig& config)
    : config_(config) {
  config_.validate();
}

void ABSClustering::configure(const HitClusteringConfig& config) {
  config.validate();
  config_ = config;
}

std::unique_ptr<IClusteringState> ABSClustering::createState() const {
  // Create state with appropriate initial capacity
  return std::make_unique<ABSClusteringState>(config_.abs.pre_allocate_buckets);
}

size_t ABSClustering::cluster(std::vector<TDCHit>::const_iterator begin,
                              std::vector<TDCHit>::const_iterator end,
                              IClusteringState& state,
                              std::vector<int>& cluster_labels) const {
  // Cast to concrete state type
  auto& abs_state = dynamic_cast<ABSClusteringState&>(state);

  const size_t num_hits = std::distance(begin, end);

  if (num_hits == 0) {
    cluster_labels.clear();
    return 0;
  }

  // Verify cluster_labels is properly sized
  if (cluster_labels.size() != num_hits) {
    throw std::invalid_argument(
        "cluster_labels size must match input hit range");
  }

  // Reset state for new clustering run
  abs_state.reset();

  // Process each hit sequentially (LOCAL indices 0, 1, 2, ...)
  size_t local_hit_index = 0;
  for (auto it = begin; it != end; ++it, ++local_hit_index) {
    const TDCHit& hit = *it;
    abs_state.hits_processed++;

    // Periodic scan for aged buckets
    if (abs_state.hits_processed % config_.abs.scan_interval == 0) {
      scanAndCloseAgedBuckets(hit.tof, abs_state, cluster_labels);
    }

    // Find compatible existing bucket among active buckets
    int bucket_index = findCompatibleBucket(hit, abs_state);

    if (bucket_index >= 0) {
      // Add to existing bucket
      abs_state.bucket_pool[bucket_index].addHit(local_hit_index, hit);
    } else {
      // No compatible bucket - get or create one
      size_t new_bucket_idx =
          getOrCreateBucket(local_hit_index, hit, abs_state);
      abs_state.active_bucket_indices.push_back(new_bucket_idx);
      addBucketToSpatialIndex(new_bucket_idx, hit, abs_state);
    }
  }

  // Final processing: close all remaining active buckets
  if (num_hits > 0) {
    auto last_hit = std::prev(end);
    // First try aging-based closure
    scanAndCloseAgedBuckets(
        last_hit->tof +
            static_cast<uint32_t>(config_.abs.neutron_correlation_window /
                                  25.0) +
            1,
        abs_state, cluster_labels);

    // Force-close any remaining active buckets (end of data)
    abs_state.remaining_buckets.assign(abs_state.active_bucket_indices.begin(),
                                       abs_state.active_bucket_indices.end());
    for (size_t bucket_idx : abs_state.remaining_buckets) {
      closeBucket(bucket_idx, abs_state, cluster_labels);
    }
  }

  return abs_state.next_cluster_id;
}

int ABSClustering::findCompatibleBucket(const TDCHit& hit,
                                        const ABSClusteringState& state) const {
  // Use spatial indexing for faster bucket lookup
  auto [bin_x, bin_y] = getSpatialBin(hit);
  const double r = config_.abs.radius;

  // Check the bin containing the hit and neighboring bins
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      int check_x = static_cast<int>(bin_x) + dx;
      int check_y = static_cast<int>(bin_y) + dy;

      if (check_x < 0 || check_x >= static_cast<int>(SPATIAL_GRID_SIZE) ||
          check_y < 0 || check_y >= static_cast<int>(SPATIAL_GRID_SIZE)) {
        continue;
      }

      const auto& bin = state.spatial_bins[check_x][check_y];
      for (size_t bucket_idx : bin.bucket_indices) {
        const ABSBucket& bucket = state.bucket_pool[bucket_idx];

        // Check temporal constraint first (cheaper)
        if (!bucket.fitsTemporally(hit,
                                   config_.abs.neutron_correlation_window)) {
          continue;
        }

        // Check spatial constraint
        if (!bucket.fitsSpatially(hit, r)) {
          continue;
        }

        return static_cast<int>(bucket_idx);
      }
    }
  }

  return -1;  // No compatible bucket found
}

size_t ABSClustering::getOrCreateBucket(size_t hit_index, const TDCHit& hit,
                                        ABSClusteringState& state) const {
  size_t bucket_idx;

  if (!state.free_bucket_indices.empty()) {
    // Reuse a free bucket
    bucket_idx = state.free_bucket_indices.back();
    state.free_bucket_indices.pop_back();
  } else {
    // All buckets are active - grow the pool
    bucket_idx = state.bucket_pool.size();
    state.bucket_pool.emplace_back();
  }

  // Initialize the bucket with the hit
  state.bucket_pool[bucket_idx].initialize(hit_index, hit);

  return bucket_idx;
}

void ABSClustering::freeBucket(size_t bucket_index,
                               ABSClusteringState& state) const {
  // Reset the bucket
  state.bucket_pool[bucket_index].reset();

  // Add to free list
  state.free_bucket_indices.push_back(bucket_index);

  // Remove from active list using swap-and-pop for O(1) removal
  auto it = std::find(state.active_bucket_indices.begin(),
                      state.active_bucket_indices.end(), bucket_index);
  if (it != state.active_bucket_indices.end()) {
    std::swap(*it, state.active_bucket_indices.back());
    state.active_bucket_indices.pop_back();
  }
}

void ABSClustering::scanAndCloseAgedBuckets(
    uint32_t reference_tof, ABSClusteringState& state,
    std::vector<int>& cluster_labels) const {
  // Collect indices of buckets to close (can't modify active list while
  // iterating) Use pre-allocated vector to avoid allocation
  state.buckets_to_close.clear();

  // Scan only active buckets
  for (size_t active_idx : state.active_bucket_indices) {
    ABSBucket& bucket = state.bucket_pool[active_idx];

    // Check if bucket is too old to receive new hits
    if (!bucket.isAged(reference_tof, config_.abs.neutron_correlation_window)) {
      continue;  // Bucket still valid
    }

    // Mark for closure
    state.buckets_to_close.push_back(active_idx);
  }

  // Close aged buckets
  for (size_t bucket_idx : state.buckets_to_close) {
    closeBucket(bucket_idx, state, cluster_labels);
  }
}

void ABSClustering::closeBucket(size_t bucket_index, ABSClusteringState& state,
                                std::vector<int>& cluster_labels) const {
  ABSBucket& bucket = state.bucket_pool[bucket_index];

  // Remove from spatial index
  removeBucketFromSpatialIndex(bucket_index, state);

  // Check if bucket has sufficient hits for valid cluster
  bool formed_cluster =
      bucket.closeBucket(config_.abs.min_cluster_size, state.next_cluster_id);

  if (formed_cluster) {
    // Valid neutron cluster - assign cluster IDs to cluster_labels vector
    for (size_t hit_index : bucket.hit_indices) {
      if (hit_index >= cluster_labels.size()) {
        // This should never happen with correct implementation
        throw std::runtime_error(
            "Hit index out of bounds in bucket - state corruption");
      }
      cluster_labels[hit_index] = bucket.cluster_label;
    }
    state.next_cluster_id++;
  } else {
    // Insufficient hits - leave as gamma noise (cluster_id = -1)
    // cluster_labels already initialized to -1, so nothing to do
  }

  // Free the bucket for reuse
  freeBucket(bucket_index, state);
}

std::pair<size_t, size_t> ABSClustering::getSpatialBin(
    const TDCHit& hit) const {
  // Simple spatial hashing: divide detector into 32x32 grid
  // Assuming 256x256 detector, each bin covers 8x8 pixels
  size_t bin_x = std::min(static_cast<size_t>(hit.x / SPATIAL_BIN_SIZE),
                          SPATIAL_GRID_SIZE - 1);
  size_t bin_y = std::min(static_cast<size_t>(hit.y / SPATIAL_BIN_SIZE),
                          SPATIAL_GRID_SIZE - 1);
  return {bin_x, bin_y};
}

void ABSClustering::addBucketToSpatialIndex(size_t bucket_idx,
                                            const TDCHit& hit,
                                            ABSClusteringState& state) const {
  auto [bin_x, bin_y] = getSpatialBin(hit);
  state.spatial_bins[bin_x][bin_y].bucket_indices.push_back(bucket_idx);
}

void ABSClustering::removeBucketFromSpatialIndex(
    size_t bucket_idx, ABSClusteringState& state) const {
  const ABSBucket& bucket = state.bucket_pool[bucket_idx];

  if (bucket.hit_indices.empty()) {
    return;  // No hits, nothing to remove
  }

  // Use center of bucket bounds for spatial bin
  int center_x = (bucket.x_min + bucket.x_max) / 2;
  int center_y = (bucket.y_min + bucket.y_max) / 2;

  // Ensure bounds
  center_x = std::max(0, std::min(255, center_x));
  center_y = std::max(0, std::min(255, center_y));

  size_t bin_x = static_cast<size_t>(center_x / SPATIAL_BIN_SIZE);
  size_t bin_y = static_cast<size_t>(center_y / SPATIAL_BIN_SIZE);

  auto& bin = state.spatial_bins[bin_x][bin_y];
  auto it = std::find(bin.bucket_indices.begin(), bin.bucket_indices.end(),
                      bucket_idx);
  if (it != bin.bucket_indices.end()) {
    // Use swap-and-pop for O(1) removal
    std::swap(*it, bin.bucket_indices.back());
    bin.bucket_indices.pop_back();
  }
}

ClusteringStatistics ABSClustering::getStatistics(const IClusteringState& state,
                                                  size_t num_hits) const {
  const auto& abs_state = dynamic_cast<const ABSClusteringState&>(state);

  ClusteringStatistics stats;
  stats.total_hits_processed = num_hits;
  stats.total_clusters = abs_state.next_cluster_id;

  // Count unclustered hits would require access to cluster_labels
  // For now, we'll estimate based on typical behavior
  stats.unclustered_hits = 0;  // Would need to count -1 labels

  if (stats.total_clusters > 0) {
    stats.mean_cluster_size = static_cast<double>(stats.total_hits_processed -
                                                  stats.unclustered_hits) /
                              stats.total_clusters;
  }

  return stats;
}

}  // namespace tdcsophiread