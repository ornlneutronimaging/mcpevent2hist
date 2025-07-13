// TDCSophiread Simple ABS Clustering Implementation
// Basic Age-Based Spatial clustering adapted to new iterator interface

#include "neutron_processing/simple_abs_clustering.h"

#include <algorithm>
#include <stdexcept>

namespace tdcsophiread {

SimpleABSClustering::SimpleABSClustering()
    : config_(),
      cluster_labels_(),
      last_hit_count_(0),
      next_cluster_id_(0),
      hits_processed_(0) {
  // Initialize with default ABS configuration
  config_.algorithm = "simple_abs";
  config_.abs.radius = 5.0;
  config_.abs.min_cluster_size = 1;
  config_.abs.neutron_correlation_window = 75.0;
  config_.abs.scan_interval = 100;

  // Pre-allocate bucket pool with initial capacity
  bucket_pool_.reserve(INITIAL_BUCKET_POOL_SIZE);
  for (size_t i = 0; i < INITIAL_BUCKET_POOL_SIZE; ++i) {
    bucket_pool_.emplace_back();
    free_bucket_indices_.push_back(i);
  }

  // Reserve space for active bucket tracking
  active_bucket_indices_.reserve(INITIAL_BUCKET_POOL_SIZE);
}

SimpleABSClustering::SimpleABSClustering(const HitClusteringConfig& config)
    : config_(config),
      cluster_labels_(),
      last_hit_count_(0),
      next_cluster_id_(0),
      hits_processed_(0) {
  config_.validate();

  // Pre-allocate bucket pool with initial capacity
  bucket_pool_.reserve(INITIAL_BUCKET_POOL_SIZE);
  for (size_t i = 0; i < INITIAL_BUCKET_POOL_SIZE; ++i) {
    bucket_pool_.emplace_back();
    free_bucket_indices_.push_back(i);
  }

  // Reserve space for active bucket tracking
  active_bucket_indices_.reserve(INITIAL_BUCKET_POOL_SIZE);
}

void SimpleABSClustering::configure(const HitClusteringConfig& config) {
  config.validate();
  config_ = config;
  reset();
}

size_t SimpleABSClustering::cluster(std::vector<TDCHit>::const_iterator begin,
                                    std::vector<TDCHit>::const_iterator end) {
  const size_t num_hits = std::distance(begin, end);

  if (num_hits == 0) {
    cluster_labels_.clear();
    last_hit_count_ = 0;
    return 0;
  }

  // Initialize cluster labels to -1 (unclustered) for LOCAL iterator range
  cluster_labels_.clear();
  cluster_labels_.resize(num_hits, -1);

  // Reset processing state (but preserve last_hit_count_)
  reset();
  last_hit_count_ = num_hits;

  // Process each hit sequentially (LOCAL indices 0, 1, 2, ...)
  size_t local_hit_index = 0;
  for (auto it = begin; it != end; ++it, ++local_hit_index) {
    const TDCHit& hit = *it;
    hits_processed_++;

    // Periodic scan for aged buckets
    if (hits_processed_ % config_.abs.scan_interval == 0) {
      scanAndCloseAgedBuckets(hit.tof);
    }

    // Find compatible existing bucket among active buckets
    int bucket_index = findCompatibleBucket(hit);

    if (bucket_index >= 0) {
      // Add to existing bucket
      bucket_pool_[bucket_index].addHit(local_hit_index, hit);
    } else {
      // No compatible bucket - get or create one
      size_t new_bucket_idx = getOrCreateBucket(local_hit_index, hit);
      active_bucket_indices_.push_back(new_bucket_idx);
    }
  }

  // Final processing: close all remaining active buckets
  if (num_hits > 0) {
    auto last_hit = std::prev(end);
    // First try aging-based closure
    scanAndCloseAgedBuckets(
        last_hit->tof +
        static_cast<uint32_t>(config_.abs.neutron_correlation_window / 25.0) +
        1);

    // Force-close any remaining active buckets (end of data)
    std::vector<size_t> remaining_buckets =
        active_bucket_indices_;  // Copy to avoid modification during iteration
    for (size_t bucket_idx : remaining_buckets) {
      closeBucket(bucket_idx);
    }
  }

  return next_cluster_id_;
}

void SimpleABSClustering::reset() {
  // Reset active and free bucket lists
  active_bucket_indices_.clear();
  free_bucket_indices_.clear();

  // All buckets become free
  for (size_t i = 0; i < bucket_pool_.size(); ++i) {
    bucket_pool_[i].reset();
    free_bucket_indices_.push_back(i);
  }

  // Reset state
  next_cluster_id_ = 0;
  hits_processed_ = 0;
  last_hit_count_ = 0;
}

std::string SimpleABSClustering::getName() const { return "simple_abs"; }

ClusteringStatistics SimpleABSClustering::getStatistics() const {
  ClusteringStatistics stats;
  stats.total_hits_processed = last_hit_count_;
  stats.total_clusters = next_cluster_id_;
  stats.unclustered_hits =
      std::count(cluster_labels_.begin(), cluster_labels_.end(), -1);

  if (stats.total_clusters > 0) {
    stats.mean_cluster_size = static_cast<double>(stats.total_hits_processed -
                                                  stats.unclustered_hits) /
                              stats.total_clusters;
  }

  return stats;
}

int SimpleABSClustering::findCompatibleBucket(const TDCHit& hit) const {
  // Only search among active buckets
  for (size_t active_idx : active_bucket_indices_) {
    const SimpleABSBucket& bucket = bucket_pool_[active_idx];

    // Check temporal constraint first (cheaper)
    if (!bucket.fitsTemporally(hit, config_.abs.neutron_correlation_window)) {
      continue;
    }

    // Check spatial constraint
    if (!bucket.fitsSpatially(hit, config_.abs.radius)) {
      continue;
    }

    return static_cast<int>(active_idx);  // Found compatible bucket
  }

  return -1;  // No compatible bucket found
}

size_t SimpleABSClustering::getOrCreateBucket(size_t hit_index,
                                              const TDCHit& hit) {
  size_t bucket_idx;

  if (!free_bucket_indices_.empty()) {
    // Reuse a free bucket
    bucket_idx = free_bucket_indices_.back();
    free_bucket_indices_.pop_back();
  } else {
    // All buckets are active - grow the pool
    bucket_idx = bucket_pool_.size();
    bucket_pool_.emplace_back();
  }

  // Initialize the bucket with the hit
  bucket_pool_[bucket_idx].initialize(hit_index, hit);

  return bucket_idx;
}

void SimpleABSClustering::freeBucket(size_t bucket_index) {
  // Reset the bucket
  bucket_pool_[bucket_index].reset();

  // Add to free list
  free_bucket_indices_.push_back(bucket_index);

  // Remove from active list
  auto it = std::find(active_bucket_indices_.begin(),
                      active_bucket_indices_.end(), bucket_index);
  if (it != active_bucket_indices_.end()) {
    active_bucket_indices_.erase(it);
  }
}

void SimpleABSClustering::scanAndCloseAgedBuckets(uint32_t reference_tof) {
  // Collect indices of buckets to close (can't modify active list while
  // iterating)
  std::vector<size_t> buckets_to_close;

  // Scan only active buckets
  for (size_t active_idx : active_bucket_indices_) {
    SimpleABSBucket& bucket = bucket_pool_[active_idx];

    // Check if bucket is aged
    if (bucket.isAged(reference_tof, config_.abs.neutron_correlation_window)) {
      buckets_to_close.push_back(active_idx);
    }
  }

  // Close aged buckets
  for (size_t bucket_idx : buckets_to_close) {
    closeBucket(bucket_idx);
  }
}

void SimpleABSClustering::closeBucket(size_t bucket_index) {
  SimpleABSBucket& bucket = bucket_pool_[bucket_index];

  if (!bucket.is_active) {
    return;  // Already closed
  }

  // Check if bucket has sufficient hits for valid cluster
  bool formed_cluster =
      bucket.closeBucket(config_.abs.min_cluster_size, next_cluster_id_);

  if (formed_cluster) {
    // Valid neutron cluster - assign cluster IDs to LOCAL cluster_labels_
    // vector
    for (size_t hit_index : bucket.hit_indices) {
      cluster_labels_[hit_index] = bucket.cluster_label;
    }
    next_cluster_id_++;
  } else {
    // Insufficient hits - leave as gamma noise (cluster_id = -1)
    for (size_t hit_index : bucket.hit_indices) {
      cluster_labels_[hit_index] = -1;
    }
  }

  // Free the bucket for reuse
  freeBucket(bucket_index);
}

}  // namespace tdcsophiread