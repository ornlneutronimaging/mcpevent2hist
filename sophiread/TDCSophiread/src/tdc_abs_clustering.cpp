// TDCSophiread ABS (Adaptive Box Search) Clustering Algorithm Implementation

#include "tdc_abs_clustering.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>

namespace tdcsophiread {

ABSClustering::ABSClustering(const ABSConfig& config)
    : config_(config), next_cluster_label_(0), active_clusters_(0) {
  config_.validate();  // Ensure configuration is valid
  reset();
}

size_t ABSClustering::fit(std::vector<TDCHit>& hits) {
  start_time_ = std::chrono::high_resolution_clock::now();

  // Reset state for new clustering operation
  reset();

  // Pre-allocate cluster labels vector
  cluster_labels_.clear();
  cluster_labels_.reserve(hits.size());

  // Process each hit sequentially
  for (size_t i = 0; i < hits.size(); ++i) {
    const TDCHit& hit = hits[i];

    // Find compatible existing cluster
    int cluster_index = findCompatibleCluster(hit);

    int32_t assigned_label;
    if (cluster_index >= 0) {
      // Add to existing cluster
      clusters_[cluster_index].addHit(hit);
      assigned_label = clusters_[cluster_index].label;
    } else {
      // Create new cluster or replace oldest
      assigned_label = createOrReplaceCluster(hit);
    }

    // Assign cluster label to hit and store
    hits[i].cluster_id = assigned_label;
    cluster_labels_.push_back(assigned_label);
  }

  // Update statistics
  updateStatistics();

  return next_cluster_label_;
}

const std::vector<int>& ABSClustering::getClusterLabels() const {
  return cluster_labels_;
}

int ABSClustering::findCompatibleCluster(const TDCHit& hit) const {
  for (size_t i = 0; i < MAX_CLUSTERS; ++i) {
    const ABSCluster& cluster = clusters_[i];

    if (!cluster.isActive()) {
      continue;  // Skip inactive clusters
    }

    // Check temporal constraint first (cheaper)
    if (!cluster.fitsTemporally(hit, config_.time_range_ns)) {
      continue;
    }

    // Check spatial constraint
    if (!cluster.fitsSpatially(hit, config_.radius)) {
      continue;
    }

    return static_cast<int>(i);  // Found compatible cluster
  }

  return -1;  // No compatible cluster found
}

int ABSClustering::findOldestCluster() const {
  int oldest_index = 0;
  uint32_t oldest_timestamp = std::numeric_limits<uint32_t>::max();

  for (size_t i = 0; i < MAX_CLUSTERS; ++i) {
    if (clusters_[i].isActive() && clusters_[i].timestamp < oldest_timestamp) {
      oldest_timestamp = clusters_[i].timestamp;
      oldest_index = static_cast<int>(i);
    }
  }

  return oldest_index;
}

int32_t ABSClustering::createOrReplaceCluster(const TDCHit& hit) {
  int cluster_index = 0;  // Initialize to silence warning

  if (active_clusters_ < MAX_CLUSTERS) {
    // Find first inactive slot
    for (size_t i = 0; i < MAX_CLUSTERS; ++i) {
      if (!clusters_[i].isActive()) {
        cluster_index = static_cast<int>(i);
        break;
      }
    }
    active_clusters_++;
  } else {
    // All slots full - replace oldest cluster (LRU strategy)
    cluster_index = findOldestCluster();
    stats_.cluster_replacements++;
  }

  // Initialize cluster with the hit
  int32_t new_label = next_cluster_label_++;
  clusters_[cluster_index].initialize(hit, new_label);

  return new_label;
}

void ABSClustering::updateStatistics() {
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time_);

  stats_.total_hits = cluster_labels_.size();
  stats_.total_clusters = next_cluster_label_;
  stats_.processing_time_ms = duration.count() / 1000.0;

  // Count cluster sizes with bounds checking
  // Prevent excessive memory allocation from corrupted cluster count
  const size_t max_reasonable_clusters =
      stats_.total_hits;  // Can't have more clusters than hits
  const size_t safe_cluster_count = std::min(
      static_cast<size_t>(next_cluster_label_), max_reasonable_clusters);

  if (safe_cluster_count != static_cast<size_t>(next_cluster_label_)) {
    std::cerr << "Warning: Cluster count (" << next_cluster_label_
              << ") exceeds hit count (" << stats_.total_hits
              << "). Using safe count: " << safe_cluster_count << std::endl;
  }

  std::vector<size_t> cluster_sizes(safe_cluster_count, 0);
  for (int label : cluster_labels_) {
    if (label >= 0 && static_cast<size_t>(label) < cluster_sizes.size()) {
      cluster_sizes[label]++;
    }
  }

  stats_.single_hit_clusters = 0;
  stats_.multi_hit_clusters = 0;
  size_t total_hits_in_clusters = 0;

  for (size_t size : cluster_sizes) {
    if (size == 1) {
      stats_.single_hit_clusters++;
    } else if (size > 1) {
      stats_.multi_hit_clusters++;
    }
    total_hits_in_clusters += size;
  }

  stats_.mean_cluster_size =
      (stats_.total_clusters > 0)
          ? static_cast<double>(total_hits_in_clusters) / stats_.total_clusters
          : 0.0;
}

ABSClustering::ClusteringStats ABSClustering::getStatistics() const {
  return stats_;
}

void ABSClustering::reset() {
  // Reset all clusters to inactive state
  for (auto& cluster : clusters_) {
    cluster.reset();
  }

  next_cluster_label_ = 0;
  active_clusters_ = 0;
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