// TDCSophiread ABS (Adaptive Box Search) Clustering Algorithm Implementation
// Physics-correct implementation with dynamic buckets and time-based aging

#include "tdc_abs_clustering.h"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace tdcsophiread {

ABSClustering::ABSClustering(const ABSConfig& config)
    : config_(config),
      hits_ptr_(nullptr),
      next_cluster_label_(0),
      hits_processed_(0) {
  config_.validate();  // Ensure configuration is valid

  // Pre-allocate bucket pool with initial capacity
  bucket_pool_.reserve(INITIAL_BUCKET_POOL_SIZE);
  for (size_t i = 0; i < INITIAL_BUCKET_POOL_SIZE; ++i) {
    bucket_pool_.emplace_back();
    free_bucket_indices_.push_back(i);
  }

  // Reserve space for active bucket tracking
  active_bucket_indices_.reserve(INITIAL_BUCKET_POOL_SIZE);

  reset();
}

size_t ABSClustering::fit(std::vector<TDCHit>& hits) {
  start_time_ = std::chrono::high_resolution_clock::now();

  // Reset state for new clustering operation
  reset();
  hits_ptr_ = &hits;

  // Process each hit sequentially
  for (size_t hit_index = 0; hit_index < hits.size(); ++hit_index) {
    const TDCHit& hit = hits[hit_index];
    hits_processed_++;

    // Periodic scan for aged buckets
    if (hits_processed_ % config_.scan_interval == 0) {
      scanAndCloseAgedBuckets(hit.tof);
    }

    // Find compatible existing bucket among active buckets
    int bucket_index = findCompatibleBucket(hit);

    if (bucket_index >= 0) {
      // Add to existing bucket
      bucket_pool_[bucket_index].addHit(hit_index, hit);
    } else {
      // No compatible bucket - get or create one
      size_t new_bucket_idx = getOrCreateBucket(hit_index, hit);
      active_bucket_indices_.push_back(new_bucket_idx);
    }
  }

  // Final processing: close all remaining active buckets
  if (!hits.empty()) {
    // First try aging-based closure
    scanAndCloseAgedBuckets(
        hits.back().tof +
        static_cast<uint32_t>(config_.neutron_correlation_window / 25.0) + 1);

    // Force-close any remaining active buckets (end of data)
    std::vector<size_t> remaining_buckets =
        active_bucket_indices_;  // Copy to avoid modification during iteration
    for (size_t bucket_idx : remaining_buckets) {
      closeBucket(bucket_idx);
    }
  }

  // Update statistics
  updateStatistics();

  return next_cluster_label_;
}

const std::vector<int>& ABSClustering::getClusterLabels() const {
  return cluster_labels_;
}

int ABSClustering::findCompatibleBucket(const TDCHit& hit) const {
  // Only search among active buckets
  for (size_t active_idx : active_bucket_indices_) {
    const ABSBucket& bucket = bucket_pool_[active_idx];

    // Check temporal constraint first (cheaper)
    if (!bucket.fitsTemporally(hit, config_.neutron_correlation_window)) {
      continue;
    }

    // Check spatial constraint
    if (!bucket.fitsSpatially(hit, config_.radius)) {
      continue;
    }

    return static_cast<int>(active_idx);  // Found compatible bucket
  }

  return -1;  // No compatible bucket found
}

size_t ABSClustering::getOrCreateBucket(size_t hit_index, const TDCHit& hit) {
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

void ABSClustering::freeBucket(size_t bucket_index) {
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

void ABSClustering::scanAndCloseAgedBuckets(uint32_t reference_tof) {
  stats_.aged_bucket_scans++;

  // Collect indices of buckets to close (can't modify active list while
  // iterating)
  std::vector<size_t> buckets_to_close;

  // Scan only active buckets
  for (size_t active_idx : active_bucket_indices_) {
    ABSBucket& bucket = bucket_pool_[active_idx];

    // Check if bucket is aged
    if (bucket.isAged(reference_tof, config_.neutron_correlation_window)) {
      buckets_to_close.push_back(active_idx);
    }
  }

  // Close aged buckets
  for (size_t bucket_idx : buckets_to_close) {
    closeBucket(bucket_idx);
  }
}

void ABSClustering::closeBucket(size_t bucket_index) {
  ABSBucket& bucket = bucket_pool_[bucket_index];

  if (!bucket.is_active || !hits_ptr_) {
    return;  // Already closed or no hit data
  }

  // Check if bucket has sufficient hits for valid cluster
  bool formed_cluster =
      bucket.closeBucket(config_.min_cluster_size, next_cluster_label_);

  if (formed_cluster) {
    // Valid neutron cluster - assign cluster IDs to hits
    for (size_t hit_index : bucket.hit_indices) {
      (*hits_ptr_)[hit_index].cluster_id = bucket.cluster_label;
    }
    next_cluster_label_++;
  } else {
    // Insufficient hits - leave as gamma noise (cluster_id = -1)
    for (size_t hit_index : bucket.hit_indices) {
      (*hits_ptr_)[hit_index].cluster_id = -1;
    }
    stats_.gamma_hits += bucket.hit_indices.size();
  }

  // Free the bucket for reuse
  freeBucket(bucket_index);
}

void ABSClustering::updateStatistics() {
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time_);

  stats_.total_hits = hits_processed_;
  stats_.total_clusters = next_cluster_label_;
  stats_.total_buckets = bucket_pool_.size();
  stats_.processing_time_ms = duration.count() / 1000.0;

  // Build cluster labels vector for interface compatibility
  cluster_labels_.clear();
  if (hits_ptr_) {
    cluster_labels_.reserve(hits_ptr_->size());
    for (const TDCHit& hit : *hits_ptr_) {
      cluster_labels_.push_back(hit.cluster_id);
    }
  }

  // Calculate mean cluster size (only for valid clusters)
  if (stats_.total_clusters > 0) {
    size_t total_clustered_hits = stats_.total_hits - stats_.gamma_hits;
    stats_.mean_cluster_size =
        static_cast<double>(total_clustered_hits) / stats_.total_clusters;
  } else {
    stats_.mean_cluster_size = 0.0;
  }
}

ABSClustering::ClusteringStats ABSClustering::getStatistics() const {
  return stats_;
}

void ABSClustering::reset() {
  // Reset active and free bucket lists
  active_bucket_indices_.clear();
  free_bucket_indices_.clear();

  // All buckets become free
  for (size_t i = 0; i < bucket_pool_.size(); ++i) {
    bucket_pool_[i].reset();
    free_bucket_indices_.push_back(i);
  }

  // Reset state
  next_cluster_label_ = 0;
  hits_processed_ = 0;
  hits_ptr_ = nullptr;
  cluster_labels_.clear();

  // Reset statistics
  stats_ = ClusteringStats{};
}

void ABSClustering::updateConfig(const ABSConfig& config) {
  config.validate();
  config_ = config;
}

void ABSClustering::configure(const ClusteringConfig& config) {
  updateConfig(config.abs);
}

std::string ABSClustering::getName() const { return "abs"; }

size_t ABSClustering::getLastHitCount() const { return stats_.total_hits; }

}  // namespace tdcsophiread